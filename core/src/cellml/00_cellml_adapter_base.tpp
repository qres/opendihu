#include "cellml/00_cellml_adapter_base.h"

#include <Python.h>  // has to be the first included header

#include <list>
#include <sstream>

#include "utility/python_utility.h"
#include "utility/petsc_utility.h"
#include "utility/string_utility.h"
#include "mesh/mesh_manager.h"

template<int nStates>
CellmlAdapterBase<nStates>::
CellmlAdapterBase(DihuContext context) :
  context_(context)
{
  PyObject *topLevelSettings = this->context_.getPythonConfig();
  specificSettings_ = PythonUtility::getOptionPyObject(topLevelSettings, "CellML");
  outputWriterManager_.initialize(specificSettings_);
  LOG(TRACE) << "CellmlAdapterBase constructor";
}

template<int nStates>
CellmlAdapterBase<nStates>::
~CellmlAdapterBase()
{
}

template<int nStates>
constexpr int CellmlAdapterBase<nStates>::
nComponents()
{
  return nStates;
}



template<int nStates>
void CellmlAdapterBase<nStates>::
initialize()
{
  LOG(TRACE) << "CellmlAdapterBase<nStates>::initialize";

  if (VLOG_IS_ON(1))
  {
    LOG(DEBUG) << "CellmlAdapterBase<nStates>::initialize querying meshManager for mesh";
    LOG(DEBUG) << "specificSettings_: ";
    PythonUtility::printDict(specificSettings_);
  }
  
  // create a mesh if there is not yet one assigned
  mesh_ = context_.meshManager()->mesh<>(specificSettings_);
  LOG(DEBUG) << "Cellml mesh has " << mesh_->nNodes() << " nodes";

  //store number of instances
  nInstances_ = mesh_->nNodes();

  stateNames_.resize(nStates);
  sourceFilename_ = PythonUtility::getOptionString(this->specificSettings_, "sourceFilename", "");
  this->scanSourceFile(this->sourceFilename_, statesInitialValues_);
  
  // add explicitely defined parameters that replace intermediates and constants
  if (!inputFileTypeOpenCMISS_)
  {
    PythonUtility::getOptionVector(this->specificSettings_, "parametersUsedAsIntermediate", parametersUsedAsIntermediate_);
    PythonUtility::getOptionVector(this->specificSettings_, "parametersUsedAsConstant", parametersUsedAsConstant_);
    nParameters_ += parametersUsedAsIntermediate_.size() + parametersUsedAsConstant_.size();
    
    LOG(DEBUG) << "parametersUsedAsIntermediate_: " << parametersUsedAsIntermediate_ 
      << ", parametersUsedAsConstant_: " << parametersUsedAsConstant_;
  }
  
  LOG(DEBUG) << "Initialize CellML with nInstances = " << nInstances_ << ", nParameters_ = " << nParameters_ 
    << ", nStates = " << nStates << ", nIntermediates = " << nIntermediates_;
    
  // allocate data vectors
  intermediates_.resize(nIntermediates_*nInstances_);
  parameters_.resize(nParameters_*nInstances_);
  LOG(DEBUG) << "size of parameters: "<<parameters_.size();
}

template<int nStates>
bool CellmlAdapterBase<nStates>::
setInitialValues(Vec& initialValues)
{
  LOG(TRACE) << "CellmlAdapterBase<nStates>::setInitialValues, sourceFilename_="<<this->sourceFilename_;
  if(PythonUtility::hasKey(this->specificSettings_, "statesInitialValues"))
  {
    LOG(DEBUG) << "set initial values from config";

    PythonUtility::getOptionVector(this->specificSettings_, "statesInitialValues", nStates, statesInitialValues_);
  }
  else if(this->sourceFilename_ != "")
  {
    LOG(DEBUG) << "set initial values from source file";
    // parsing the source file was already done, the initial values are stored in the statesInitialValues_ vector
  }
  else
  {
    LOG(DEBUG) << "initialize to zero";
    statesInitialValues_.resize(nStates*nInstances_, 0);
  }

  if(PythonUtility::hasKey(specificSettings_, "parametersInitialValues"))
  {
    LOG(DEBUG) << "load parametersInitialValues also from config";

    std::vector<double> parametersInitial;
    PythonUtility::getOptionVector(specificSettings_, "parametersInitialValues", nStates, parametersInitial);

    if (parametersInitial.size() == parameters_.size())
    {
      std::copy(parametersInitial.begin(), parametersInitial.end(), parameters_.begin());
      LOG(DEBUG) << "parameters size is matching for all instances";
    }
    else
    {
      LOG(DEBUG) << "copy parameters which were given only for one instance to all instances";
      for(int instanceNo=0; instanceNo<nInstances_; instanceNo++)
      {
        for(int j=0; j<nParameters_; j++)
        {
          parameters_[j*nInstances_ + instanceNo] = parametersInitial[j];
        }
      }
    }
  }
  else
  {
    LOG(DEBUG) << "Config does not contain key \"parametersInitialValues\"";
  }

  std::vector<double> statesAllInstances(nStates*nInstances_);
  for(int j=0; j<nStates; j++)
  {
    for(int instanceNo=0; instanceNo<nInstances_; instanceNo++)
    {
      statesAllInstances[j*nInstances_ + instanceNo] = statesInitialValues_[j];
    }
  }

  PetscUtility::setVector(statesAllInstances, initialValues);

  if (VLOG_IS_ON(2))
  {
    VLOG(2) << "initial values were set as follows: ";
    for(auto value : statesAllInstances)
      VLOG(2) << "  " << value;
  }
  return true;

  LOG(DEBUG) << "do not set initial values";

  return false;
}

template<int nStates>
std::shared_ptr<Mesh::Mesh> CellmlAdapterBase<nStates>::
mesh()
{
  return mesh_;
}

template<int nStates>
void CellmlAdapterBase<nStates>::
getNumbers(int& nInstances, int& nIntermediates, int& nParameters)
{
  nInstances = nInstances_;
  nIntermediates = nIntermediates_;
  nParameters = nParameters_;
}

template<int nStates>
void CellmlAdapterBase<nStates>::
getStateNames(std::vector<std::string> &stateNames)
{
  stateNames = this->stateNames_;
}

template<int nStates>
bool CellmlAdapterBase<nStates>::
knowsMeshType()
{
  return false;
}

