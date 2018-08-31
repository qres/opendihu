#include "time_stepping_scheme/time_stepping_scheme_ode.h"

#include <Python.h>  // has to be the first included header
#include <vector>

#include "utility/python_utility.h"

namespace TimeSteppingScheme
{

template<typename DiscretizableInTimeType>
TimeSteppingSchemeOde<DiscretizableInTimeType>::
TimeSteppingSchemeOde(DihuContext context, const std::string name) :
  TimeSteppingScheme(context), discretizableInTime_(context[name]), initialized_(false)
{
}

template<typename DiscretizableInTimeType>
Data::TimeStepping<typename DiscretizableInTimeType::FunctionSpace, DiscretizableInTimeType::nComponents()> &TimeSteppingSchemeOde<DiscretizableInTimeType>::
data()
{
  return *data_;
}

template<typename DiscretizableInTimeType>
void TimeSteppingSchemeOde<DiscretizableInTimeType>::
setInitialValues()
{
  // set initial values as given in settings, or set to zero if not given
  std::vector<double> localValues;
  
  bool inputMeshIsGlobal = PythonUtility::getOptionBool(this->specificSettings_, "inputMeshIsGlobal", true);
  if (inputMeshIsGlobal)
  {
    const int nDofsGlobal = this->data_->functionSpace()->nDofsGlobal();
    PythonUtility::getOptionVector(this->specificSettings_, "initialValues", nDofsGlobal, localValues);

    //std::shared_ptr<Mesh::Mesh> mesh = discretizableInTime_.mesh();
    this->data_->functionSpace()->meshPartition()->extractLocalDofsWithoutGhosts(localValues);
  }
  else 
  {
    const int nDofsLocal = this->data_->functionSpace()->nDofsLocalWithoutGhosts();
    PythonUtility::getOptionVector(this->specificSettings_, "initialValues", nDofsLocal, localValues);
  }
  //LOG(DEBUG) << "set initial values to " << values;

  // set the first component of the solution variable by the given values
  data_->solution().setValuesWithoutGhosts(0, localValues);
}

template<typename DiscretizableInTimeType>
SolutionVectorMapping &TimeSteppingSchemeOde<DiscretizableInTimeType>::
solutionVectorMapping()
{
  return discretizableInTime_.solutionVectorMapping();
}

template<typename DiscretizableInTimeType>
typename TimeSteppingSchemeOde<DiscretizableInTimeType>::Data::FieldVariableType &TimeSteppingSchemeOde<DiscretizableInTimeType>::
solution()
{
  return data_->solution();
}

template<typename DiscretizableInTimeType>
void TimeSteppingSchemeOde<DiscretizableInTimeType>::
setRankSubset(Partition::RankSubset rankSubset)
{
  data_->setRankSubset(rankSubset);
  discretizableInTime_.setRankSubset(rankSubset);
} 
 
template<typename DiscretizableInTimeType>
void TimeSteppingSchemeOde<DiscretizableInTimeType>::
reset()
{
  TimeSteppingScheme::reset();
  discretizableInTime_.reset();
  
  initialized_ = false;
}

template<typename DiscretizableInTimeType>
void TimeSteppingSchemeOde<DiscretizableInTimeType>::
initialize()
{
  if (initialized_)
    return;
 
  TimeSteppingScheme::initialize();
  LOG(TRACE) << "TimeSteppingSchemeOde::initialize";

  // initialize underlying DiscretizableInTime object, also with time step width
  discretizableInTime_.initialize();
  discretizableInTime_.initialize(this->timeStepWidth_);

  std::shared_ptr<Mesh::Mesh> mesh = discretizableInTime_.mesh();
  data_->setFunctionSpace(std::static_pointer_cast<typename DiscretizableInTimeType::FunctionSpace>(mesh));
  
  // set component names in data
  std::vector<std::string> componentNames;
  discretizableInTime_.getComponentNames(componentNames);
  data_->setComponentNames(componentNames);
  
  data_->initialize();

  timeStepOutputInterval_ = PythonUtility::getOptionInt(specificSettings_, "timeStepOutputInterval", 100, PythonUtility::Positive);

  // set initial values from settings

  // load initial values as specified in config under the "CellML" section
  if (!discretizableInTime_.setInitialValues(data_->solution()))
  {
    // if it did not initialize it,
    // load initial values from config under the timestepping section
    this->setInitialValues();
  }
  
  data_->print();
  
  initialized_ = true;
}

template<typename DiscretizableInTimeType>
void TimeSteppingSchemeOde<DiscretizableInTimeType>::
run()
{
  // initialize
  this->initialize();

  // do simulations
  this->advanceTimeSpan();
}

template<typename DiscretizableInTimeType>
bool TimeSteppingSchemeOde<DiscretizableInTimeType>::
knowsMeshType()
{
  return this->discretizableInTime_.knowsMeshType();
}


} // namespace
