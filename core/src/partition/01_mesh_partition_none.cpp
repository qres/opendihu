#include "partition/01_mesh_partition.h"

#include <petscsys.h>

#include "mesh/mesh.h"
#include "utility/mpi_utility.h"

namespace Partition
{

MeshPartition<Mesh::None>::
MeshPartition(global_no_t globalSize, std::shared_ptr<RankSubset> rankSubset) :
  MeshPartitionBase(rankSubset), globalSize_(globalSize)
{
  // get the own current MPI rank
  int rankNo;
  int nRanks;
  MPIUtility::handleReturnValue (MPI_Comm_size(this->mpiCommunicator(), &nRanks));
  MPIUtility::handleReturnValue (MPI_Comm_rank(this->mpiCommunicator(), &rankNo));
  
  global_no_t sizePerRank = globalSize_ / nRanks;
  global_no_t residual = globalSize_ - sizePerRank*nRanks;
  localSize_ = element_no_t(sizePerRank);
  
  if (rankNo < residual)
    localSize_++;
  
  beginGlobal_ = rankNo * sizePerRank + std::min(residual, (global_no_t)rankNo);
  
  // initialize local dofs list 
  this->initializeLocalNodeNos(localSize_);
}

//! get the local to global mapping for the current partition
ISLocalToGlobalMapping MeshPartition<Mesh::None>::
localToGlobalMapping()
{
  PetscErrorCode ierr;
  std::vector<PetscInt> globalDofNos(localSize());
  std::iota(globalDofNos.begin(), globalDofNos.end(), beginGlobal_);
  ISLocalToGlobalMapping localToGlobalMapping;
  ierr = ISLocalToGlobalMappingCreate(mpiCommunicator(), 1, localSize(), 
                                      globalDofNos.data(), PETSC_COPY_VALUES, &localToGlobalMapping); CHKERRABORT(mpiCommunicator(), ierr);

  return localToGlobalMapping;
}
 
//! number of entries in the current partition
element_no_t MeshPartition<Mesh::None>::
localSize()
{
  return localSize_;
}

//! number of entries in total
global_no_t MeshPartition<Mesh::None>::
globalSize()
{
  return globalSize_;
}
    
void MeshPartition<Mesh::None>::
extractLocalDofs(std::vector<double> &vector)
{
  extractLocalNodes<double>(vector);
}
  
void MeshPartition<Mesh::None>::
output(std::ostream &stream)
{
  stream << "MeshPartition<None>, size global: " << globalSize_ << ", local: " << localSize_ << ", beginGlobal: " << beginGlobal_;
}


}  // namespace

// output local to global mapping
std::ostream &operator<<(std::ostream &stream, std::shared_ptr<ISLocalToGlobalMapping> localToGlobalMapping)
{
  stream << *localToGlobalMapping;
  return stream;
}

// output local to global mapping
std::ostream &operator<<(std::ostream &stream, ISLocalToGlobalMapping localToGlobalMapping)
{
  PetscErrorCode ierr;
  PetscInt nproc;
  PetscInt *procs;
  PetscInt *numprocs;
  PetscInt **indices;
  ierr = ISLocalToGlobalMappingGetInfo(localToGlobalMapping, &nproc, &procs, &numprocs, &indices); CHKERRABORT(MPI_COMM_WORLD, ierr);
  stream << "[localToGlobalMapping, number of foreign ranks: " << nproc << ": (";
  
  // output ranks
  if (nproc > 0)
  {
    stream << procs[0];
  }
  for (int i = 1; i < nproc; i++)
  {
    stream << ", " << procs[i];
  }
  stream << "), number of indices on ranks: (";
  
  // output number of indices on ranks
  PetscInt numprocmax = 0;
  if (nproc > 0)
  {
    numprocmax = std::max(numprocmax, numprocs[0]);
    stream << numprocs[0];
  }
  for (int i = 1; i < nproc; i++)
  {
    numprocmax = std::max(numprocmax, numprocs[i]);
    stream << ", " << numprocs[i];
  }
  stream << "), local indices of boundary elements: (";
  
  // output boundary elements
  if (nproc > 0)
  {
    for (int j = 0; j < numprocmax; j++) 
    {
      stream << indices[0][j] << " ";
    }
  }
  for (int i = 1; i < nproc; i++)
  {
    for (int j = 0; j < numprocmax; j++) 
    {
      stream << "; " << indices[i][j];
    }
  }
  stream << "), ";
  
  PetscInt nLocalIndices;
  ierr = ISLocalToGlobalMappingGetSize(localToGlobalMapping, &nLocalIndices); CHKERRABORT(MPI_COMM_WORLD, ierr);
  stream << nLocalIndices << " local indices: [";
  
  PetscInt const *localIndices;
  ierr = ISLocalToGlobalMappingGetIndices(localToGlobalMapping, &localIndices); CHKERRABORT(MPI_COMM_WORLD, ierr);
  
  if (nLocalIndices > 0)
  {
    stream << localIndices[0];
  }
  for (int i = 1; i < nLocalIndices; i++)
  {
    stream << "," << localIndices[i];
  }
  stream << "]]";
  
  return stream;
}