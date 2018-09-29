#include "output_writer/paraview/paraview.h"

#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>

#include "easylogging++.h"
#include "base64.h"

#include "output_writer/paraview/loop_output.h"
#include "output_writer/paraview/loop_collect_mesh_properties.h"
#include "output_writer/paraview/loop_get_nodal_values.h"
#include "output_writer/paraview/loop_get_geometry_field_nodal_values.h"
#include "output_writer/paraview/poly_data_properties_for_mesh.h"

namespace OutputWriter
{

template<typename T>
void Paraview::writeCombinedValuesVector(MPI_File fileHandle, int ownRankNo, MPI_Comm mpiCommunicator, const std::vector<T> &values)
{
  // fill the write buffer with the local values
  std::string writeBuffer;
  if (binaryOutput_)
  {
    // gather data length of total vector to rank 0
    int localValuesSize = values.size() * sizeof(float);  // number of bytes
    std::vector<int> globalValuesSize(1);
    MPIUtility::handleReturnValue(MPI_Gather(&localValuesSize, 1, MPI_INT, globalValuesSize.data(), 1, MPI_INT, 0,
                                              mpiCommunicator));

    // get the encoded data from the values vector
    std::string stringData = Paraview::encodeBase64(values, false);  //without leading dataset size

    // on rank 0 prepend the encoded total length of the data vector in bytes
    if (ownRankNo == 0)
    {
      writeBuffer = Paraview::encodeBase64(globalValuesSize, false);  //without leading dataset size
      writeBuffer += stringData;
    }
    else
    {
      writeBuffer = stringData;
    }
  }
  else
  {
    writeBuffer = Paraview::convertToAscii(values, fixedFormat_);
  }

  MPIUtility::handleReturnValue(MPI_File_write_ordered(fileHandle, writeBuffer.c_str(), writeBuffer.length(), MPI_BYTE, MPI_STATUS_IGNORE), "MPI_File_write_ordered");
}

template<typename OutputFieldVariablesType>
void Paraview::writePolyDataFile(const OutputFieldVariablesType &fieldVariables, std::set<std::string> &combinedMeshesOut)
{
  // output a *.vtp file which contains 1D meshes, if there are any

  // collect the size data that is needed to compute offsets for parallel file output
  std::map<std::string, PolyDataPropertiesForMesh> meshProperties;
  ParaviewLoopOverTuple::loopCollectMeshProperties<OutputFieldVariablesType>(fieldVariables, meshProperties);

  /*
   PolyDataPropertiesForMesh:
  int dimensionality;    ///< D=1: object is a VTK "Line" and can be written to a combined vtp file
  global_no_t nPoints;   ///< the number of points needed for representing the mesh, note that some points may be
                         ///< duplicated because they are ghosts on one process and non-ghost on the other. This is intended.
  global_no_t nCells;    ///< the number of VTK "cells", i.e. "Lines", which is the opendihu number of "1D elements"

  std::vector<std::pair<std::string,int>> pointDataArrays;   ///< <name,nComponents> of PointData DataArray elements
  */

  /* one VTKPiece is the XML element that will be output as <Piece></Piece>. It is created from one or multiple opendihu meshes
   */
  struct VTKPiece
  {
    std::set<std::string> meshNamesCombinedMeshes;   ///< the meshNames of the combined meshes, or only one meshName if it is not a merged mesh
    PolyDataPropertiesForMesh properties;   ///< the properties of the merged mesh

    std::string firstScalarName;   ///< name of the first scalar field variable of the mesh
    std::string firstVectorName;   ///< name of the first non-scalar field variable of the mesh

    //! constructor, initialize nPoints and nCells to 0
    VTKPiece()
    {
      properties.nPoints = 0;
      properties.nCells = 0;
    }

    //! assign the correct values to firstScalarName and firstVectorName, only if properties has been set
    void setVTKValues()
    {
      // set values for firstScalarName and firstVectorName from the values in pointDataArrays
      for (auto pointDataArray : properties.pointDataArrays)
      {
        if (firstScalarName == "" && pointDataArray.second == 1)
          firstScalarName = pointDataArray.first;
        if (firstVectorName == "" && pointDataArray.second != 1)
          firstVectorName = pointDataArray.first;
      }
    }
  } vtkPiece;

  // parse the collected properties of the meshes that will be output to the file
  for (std::map<std::string,PolyDataPropertiesForMesh>::iterator iter = meshProperties.begin(); iter != meshProperties.end(); iter++)
  {
    std::string meshName = iter->first;

    // do not combine meshes other than 1D meshes
    if (vtkPiece.properties.dimensionality != 1)
      continue;

    // check if this mesh should be combined with other meshes
    bool combineMesh = true;

    // check if mesh can be merged into previous meshes
    if (!vtkPiece.properties.pointDataArrays.empty())   // if properties are already assigned by an earlier mesh
    {
      if (vtkPiece.properties.pointDataArrays.size() != iter->second.pointDataArrays.size())
      {
        LOG(DEBUG) << "Mesh " << meshName << " cannot be combined with " << vtkPiece.meshNamesCombinedMeshes << ". Number of field variables mismatches for "
          << meshName << " (is " << iter->second.pointDataArrays.size() << " instead of " << vtkPiece.properties.pointDataArrays.size() << ")";
        combineMesh = false;
      }
      else
      {
        for (int j = 0; j < iter->second.pointDataArrays.size(); j++)
        {
          if (vtkPiece.properties.pointDataArrays[j].first != iter->second.pointDataArrays[j].first)  // if the name of the jth field variable is different
          {
            LOG(DEBUG) << "Mesh " << meshName << " cannot be combined with " << vtkPiece.meshNamesCombinedMeshes << ". Field variable names mismatch for "
              << meshName << " (there is \"" << vtkPiece.properties.pointDataArrays[j].first << "\" instead of \"" << iter->second.pointDataArrays[j].first << "\")";
            combineMesh = false;
          }
        }
      }

      if (combineMesh)
      {
        LOG(DEBUG) << "Combine mesh " << meshName << " with " << vtkPiece.meshNamesCombinedMeshes << ", add " << iter->second.nPoints << " points, " << iter->second.nCells << " elements to "
          << vtkPiece.properties.nPoints << " points, " << vtkPiece.properties.nCells << " elements";

        vtkPiece.properties.nPoints += iter->second.nPoints;
        vtkPiece.properties.nCells += iter->second.nCells;
        vtkPiece.setVTKValues();
      }
    }
    else
    {
      // properties are not yet assigned
      vtkPiece.properties = iter->second; // store properties
      vtkPiece.setVTKValues();
    }

    if (combineMesh)
    {
      vtkPiece.meshNamesCombinedMeshes.insert(meshName);
    }
  }

  combinedMeshesOut = vtkPiece.meshNamesCombinedMeshes;

  // exchange information about offset in terms of nCells and nPoints
  int nCellsPreviousRanks = 0;
  int nPointsPreviousRanks = 0;

  std::shared_ptr<Partition::RankSubset> rankSubsetAllComputedInstances = this->context_.partitionManager()->rankSubsetForCollectiveOperations();

  MPIUtility::handleReturnValue(MPI_Exscan(&vtkPiece.properties.nCells, &nCellsPreviousRanks, 1, MPI_INT, MPI_SUM, rankSubsetAllComputedInstances->mpiCommunicator()));
  MPIUtility::handleReturnValue(MPI_Exscan(&vtkPiece.properties.nPoints, &nPointsPreviousRanks, 1, MPI_INT, MPI_SUM, rankSubsetAllComputedInstances->mpiCommunicator()));

  // get local data values
  // setup connectivity array
  std::vector<int> connectivityValues(2*vtkPiece.properties.nCells);
  for (int i = 0; i < vtkPiece.properties.nCells; i++)
  {
    connectivityValues[2*i + 0] = nPointsPreviousRanks + i;
    connectivityValues[2*i + 1] = nPointsPreviousRanks + i+1;
  }

  // setup offset array
  std::vector<int> offsetValues(vtkPiece.properties.nCells);
  for (int i = 0; i < vtkPiece.properties.nCells; i++)
  {
    offsetValues[i] = nCellsPreviousRanks + 2*i + 1;
  }

  // collect all data for the field variables
  std::vector<std::vector<double>> fieldVariableValues;
  ParaviewLoopOverTuple::loopGetNodalValues<OutputFieldVariablesType>(fieldVariables, vtkPiece.meshNamesCombinedMeshes, fieldVariableValues);

  assert(fieldVariableValues.size() == vtkPiece.properties.pointDataArrays.size());

  // collect all data for the geometry field variable
  std::vector<double> geometryFieldValues;
  ParaviewLoopOverTuple::loopGetGeometryFieldNodalValues<OutputFieldVariablesType>(fieldVariables, vtkPiece.meshNamesCombinedMeshes, geometryFieldValues);

  // only continue if there is data to reduce
  if (vtkPiece.meshNamesCombinedMeshes.empty())
  {
    LOG(DEBUG) << "There are no 1D meshes that could be combined.";
  }

  LOG(DEBUG) << "Combined mesh from " << vtkPiece.meshNamesCombinedMeshes << " is a vtk piece with " << vtkPiece.properties.nPoints << " points and " << vtkPiece.properties.nCells
    << " cells";

  int nOutputFileParts = 4 + vtkPiece.properties.pointDataArrays.size();

  // create the basic structure of the output file
  std::stringstream outputFileParts[nOutputFileParts];
  int outputFilePartNo = 0;
  outputFileParts[outputFilePartNo] << "<?xml version=\"1.0\"?>" << std::endl
    << "<VTKFile type=\"PolyData\" version=\"1.0\" byte_order=\"LittleEndian\">" << std::endl    // intel cpus are LittleEndian
    << std::string(1, '\t') << "<PolyData>" << std::endl;

  outputFileParts[outputFilePartNo] << std::string(2, '\t') << "<Piece NumberOfPoints=\"" << vtkPiece.properties.nPoints << "\" NumberOfVerts=\"0\" "
    << "NumberOfLines=\"" << vtkPiece.properties.nCells << "\" NumberOfStrips=\"0\" NumberOfPolys=\"0\">" << std::endl
    << std::string(3, '\t') << "<PointData";

  if (vtkPiece.firstScalarName != "")
  {
    outputFileParts[outputFilePartNo] << " Scalars=\"" << vtkPiece.firstScalarName << "\"";
  }
  if (vtkPiece.firstVectorName != "")
  {
    outputFileParts[outputFilePartNo] << " Vectors=\"" << vtkPiece.firstVectorName << "\"";
  }
  outputFileParts[outputFilePartNo] << ">";

  // loop over field variables (PointData)
  for (std::vector<std::pair<std::string,int>>::iterator pointDataArrayIter = vtkPiece.properties.pointDataArrays.begin(); pointDataArrayIter != vtkPiece.properties.pointDataArrays.end(); pointDataArrayIter++)
  {
    // write normal data element
    outputFileParts[outputFilePartNo] << std::string(4, '\t') << "<DataArray "
        << "Name=\"" << pointDataArrayIter->first << "\" "
        << "type=\"Float32\" "
        << "NumberOfComponents=\"" << pointDataArrayIter->second << "\" format=\"" << (binaryOutput_? "binary" : "ascii")
        << "\" >" << std::endl << std::string(5, '\t');

    // at this point the data of the field variable is missing
    outputFilePartNo++;

    outputFileParts[outputFilePartNo] << std::endl << std::string(4, '\t') << "</DataArray>";
  }

  outputFileParts[outputFilePartNo] << std::string(3, '\t') << "</PointData>" << std::endl
    << std::string(3, '\t') << "<CellData>" << std::endl
    << std::string(3, '\t') << "</CellData>" << std::endl
    << std::string(3, '\t') << "<Points>" << std::endl
    << std::string(4, '\t') << "<DataArray NumberOfComponents=\"3\" format=\"" << (binaryOutput_? "binary" : "ascii")
    << "\" >" << std::endl << std::string(5, '\t');

  // at this point the data of points (geometry field) is missing
  outputFilePartNo++;

  outputFileParts[outputFilePartNo] << std::string(4, '\t') << "</DataArray>" << std::endl
    << std::string(3, '\t') << "</Points>" << std::endl
    << std::string(3, '\t') << "<Verts></Verts>" << std::endl
    << std::string(3, '\t') << "<Lines>" << std::endl
    << std::string(4, '\t') << "<DataArray Name=\"connectivity\" type=\"Int32\" "
    << (binaryOutput_? "format=\"binary\"" : "format=\"ascii\"") << ">" << std::endl << std::string(5, '\t');

  // at this point the the structural information of the lines (connectivity) is missing
  outputFilePartNo++;

  outputFileParts[outputFilePartNo]
    << std::endl << std::string(4, '\t') << "</DataArray>" << std::endl
    << std::string(4, '\t') << "<DataArray Name=\"offsets\" type=\"Int32\" "
    << (binaryOutput_? "format=\"binary\"" : "format=\"ascii\"") << ">" << std::endl << std::string(5, '\t');

  // at this point the offset array will be written to the file
  outputFilePartNo++;

  outputFileParts[outputFilePartNo]
    << std::endl << std::string(4, '\t') << "</DataArray>" << std::endl;

  outputFileParts[outputFilePartNo] << "</Lines>" << std::endl
    << std::string(3, '\t') << "<Strips></Strips>" << std::endl
    << std::string(3, '\t') << "<Polys></Polys>" << std::endl
    << std::string(2, '\t') << "</Piece>" << std::endl
    << std::string(1, '\t') << "</PolyData>" << std::endl
    << "</VTKFile>" << std::endl;

  assert(outputFilePartNo+1 == nOutputFileParts);

  // loop over output file parts and collect the missing data for the own rank

  // determine filename
  std::stringstream filename;
  filename << this->filename_ << ".vtp";

  // open file
  MPI_File fileHandle;
  MPIUtility::handleReturnValue(MPI_File_open(rankSubsetAllComputedInstances->mpiCommunicator(), filename.str().c_str(),
                                              MPI_MODE_WRONLY | MPI_MODE_CREATE | MPI_MODE_UNIQUE_OPEN, MPI_INFO_NULL, &fileHandle), "MPI_File_open");

  // write beginning of file on rank 0
  int ownRankNo = rankSubsetAllComputedInstances->ownRankNo();
  outputFilePartNo = 0;

  writeAsciiDataShared(fileHandle, ownRankNo, outputFileParts[outputFilePartNo].str());
  outputFilePartNo++;

  // get current file position
  MPI_Offset currentFilePosition = 0;
  MPIUtility::handleReturnValue(MPI_File_get_position_shared(fileHandle, &currentFilePosition), "MPI_File_get_position_shared");
  LOG(DEBUG) << "current shared file position: " << currentFilePosition;

  // write field variables
  // loop over field variables
  int fieldVariableNo = 0;
  for (std::vector<std::pair<std::string,int>>::iterator pointDataArrayIter = vtkPiece.properties.pointDataArrays.begin();
       pointDataArrayIter != vtkPiece.properties.pointDataArrays.end(); pointDataArrayIter++, fieldVariableNo++)
  {
    // write values
    writeCombinedValuesVector(fileHandle, ownRankNo, rankSubsetAllComputedInstances->mpiCommunicator(), fieldVariableValues[fieldVariableNo]);

    // write next xml constructs
    writeAsciiDataShared(fileHandle, ownRankNo, outputFileParts[outputFilePartNo].str());
    outputFilePartNo++;
  }

  // write geometry field data
  writeCombinedValuesVector(fileHandle, ownRankNo, rankSubsetAllComputedInstances->mpiCommunicator(), geometryFieldValues);

  // write next xml constructs
  writeAsciiDataShared(fileHandle, ownRankNo, outputFileParts[outputFilePartNo].str());
  outputFilePartNo++;

  // write connectivity values
  writeCombinedValuesVector(fileHandle, ownRankNo, rankSubsetAllComputedInstances->mpiCommunicator(), connectivityValues);

  // write next xml constructs
  writeAsciiDataShared(fileHandle, ownRankNo, outputFileParts[outputFilePartNo].str());
  outputFilePartNo++;

  // write offset values
  writeCombinedValuesVector(fileHandle, ownRankNo, rankSubsetAllComputedInstances->mpiCommunicator(), offsetValues);

  // write next xml constructs
  writeAsciiDataShared(fileHandle, ownRankNo, outputFileParts[outputFilePartNo].str());

  /*
    int array_of_sizes[1];
    array_of_sizes[0]=numProcs;
    int array_of_subsizes[1];
    array_of_subsizes[0]=1;
    int array_of_starts[1];
    array_of_starts[0]=myId;


    MPI_Datatype accessPattern;
    MPI_Type_create_subarray(1,array_of_sizes, array_of_subsizes, array_of_starts, MPI_ORDER_C, MPI_BYTE, &accessPattern);
    MPI_Type_commit(&accessPattern);

    MPI_File_set_view(fh, 0, MPI_BYTE, accessPattern, "native", MPI_INFO_NULL);
    MPI_File_write(fh, v, size, MPI_BYTE, MPI_STATUS_IGNORE);
  */

  MPIUtility::handleReturnValue(MPI_File_close(&fileHandle), "MPI_File_close");
}

};  // namespace
