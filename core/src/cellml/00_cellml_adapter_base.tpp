#include "cellml/00_cellml_adapter_base.h"

#include <Python.h>  // has to be the first included header

#include <list>
#include <sstream>

#include "utility/python_utility.h"
#include "utility/petsc_utility.h"
#include "utility/string_utility.h"
#include "data_management/output_connector_data.h"
#include "mesh/mesh_manager/mesh_manager.h"
#include "control/diagnostic_tool/solver_structure_visualizer.h"

template<int nStates_, int nAlgebraics_, typename FunctionSpaceType>
std::array<double,nStates_> CellmlAdapterBase<nStates_,nAlgebraics_,FunctionSpaceType>::statesInitialValues_;

template<int nStates_, int nAlgebraics_, typename FunctionSpaceType>
bool CellmlAdapterBase<nStates_,nAlgebraics_,FunctionSpaceType>::statesInitialValuesinitialized_ = false;

template<int nStates_, int nAlgebraics_, typename FunctionSpaceType>
CellmlAdapterBase<nStates_,nAlgebraics_,FunctionSpaceType>::
CellmlAdapterBase(DihuContext context) :
  context_(context), specificSettings_(PythonConfig(context_.getPythonConfig(), "CellML")),
  data_(context_), cellmlSourceCodeGenerator_()
{
  outputWriterManager_.initialize(this->context_, specificSettings_);
  LOG(TRACE) << "CellmlAdapterBase constructor";
}

template<int nStates_, int nAlgebraics_, typename FunctionSpaceType>
CellmlAdapterBase<nStates_,nAlgebraics_,FunctionSpaceType>::
CellmlAdapterBase(DihuContext context, bool initializeOutputWriter) :
  context_(context), specificSettings_(PythonConfig(context_.getPythonConfig(), "CellML")),
  data_(context_), cellmlSourceCodeGenerator_()
{
}

template<int nStates_, int nAlgebraics_, typename FunctionSpaceType>
CellmlAdapterBase<nStates_,nAlgebraics_,FunctionSpaceType>::
~CellmlAdapterBase()
{
}

template<int nStates_, int nAlgebraics_, typename FunctionSpaceType>
constexpr int CellmlAdapterBase<nStates_,nAlgebraics_,FunctionSpaceType>::
nComponents()
{
  return nStates_;
}

template<int nStates_, int nAlgebraics_, typename FunctionSpaceType>
constexpr int CellmlAdapterBase<nStates_,nAlgebraics_,FunctionSpaceType>::
nStates()
{
  return nStates_;
}

template<int nStates_, int nAlgebraics_, typename FunctionSpaceType>
void CellmlAdapterBase<nStates_,nAlgebraics_,FunctionSpaceType>::
setSolutionVariable(std::shared_ptr<FieldVariableStates> states)
{
  this->data_.setStatesVariable(states);
}

template<int nStates_, int nAlgebraics_, typename FunctionSpaceType>
void CellmlAdapterBase<nStates_,nAlgebraics_,FunctionSpaceType>::
setOutputConnectorData(std::shared_ptr<::Data::OutputConnectorData<FunctionSpaceType,nStates_>> outputConnectorDataTimeStepping)
{
  // add all state and algebraic values for transfer (option "algebraicsForTransfer"), which are stored in this->data_.getOutputConnectorData()
  // at the end of outputConnectorDataTimeStepping

  // loop over states that should be transferred
  for (typename std::vector<::Data::ComponentOfFieldVariable<FunctionSpaceType,nStates_>>::iterator iter
    = this->data_.getOutputConnectorData()->variable1.begin(); iter != this->data_.getOutputConnectorData()->variable1.end(); iter++)
  {
    // skip the first "states" entry of statesToTransfer, because this is the solution variable of the timestepping scheme and therefore
    // the timestepping scheme has already added it to the outputConnectorDataTimeStepping object
    if (iter == this->data_.getOutputConnectorData()->variable1.begin())
      continue;

    int componentNo = iter->componentNo;
    std::shared_ptr<FieldVariable::FieldVariable<FunctionSpaceType,nStates_>> values = iter->values;

    values->setRepresentationGlobal();

    // The state field variables have 'nStates_' components and can be reused.
    std::string name = values->componentName(componentNo);
    LOG(DEBUG) << "CellmlAdapterBase::setOutputConnectorData add FieldVariable " << *values << " for state " << componentNo << "," << name;

    // add this component to outputConnector of data time stepping
    outputConnectorDataTimeStepping->addFieldVariable(values, componentNo);
  }

  // loop over algebraics that should be transferred
  for (typename std::vector<::Data::ComponentOfFieldVariable<FunctionSpaceType,nAlgebraics_>>::iterator iter
    = this->data_.getOutputConnectorData()->variable2.begin(); iter != this->data_.getOutputConnectorData()->variable2.end(); iter++)
  {
    int componentNo = iter->componentNo;
    std::shared_ptr<FieldVariable::FieldVariable<FunctionSpaceType,nAlgebraics_>> values = iter->values;

    values->setRepresentationGlobal();

    // The algebraic field variables have 'nAlgebraics_' components, but the field variables in the outputConnectorDataTimeStepping object
    // have only 1 component. Therefore, we create new field variables with 1 components each that reuse the Petsc Vec's of the algebraic field variables.

    // get the parameters to create the new field variable
    std::string name = values->componentName(componentNo);
    const std::vector<std::string> componentNames{values->componentName(componentNo)};
    const bool reuseData = true;

    // create the new field variable with only the one component
    std::shared_ptr<FieldVariable::FieldVariable<FunctionSpaceType,1>> newFieldVariable
      = std::make_shared<FieldVariable::FieldVariable<FunctionSpaceType,1>>(*values, name, componentNames, reuseData);

    LOG(DEBUG) << "CellmlAdapterBase::setOutputConnectorData add FieldVariable " << newFieldVariable << " for algebraic " << componentNo << "," << name;

    // add this component to outputConnector of data time stepping
    outputConnectorDataTimeStepping->addFieldVariable2(newFieldVariable);
  }
}

template<int nStates_, int nAlgebraics_, typename FunctionSpaceType>
void CellmlAdapterBase<nStates_,nAlgebraics_,FunctionSpaceType>::
initialize()
{
  LOG(TRACE) << "CellmlAdapterBase<nStates_,nAlgebraics_,FunctionSpaceType>::initialize";

  if (VLOG_IS_ON(1))
  {
    LOG(DEBUG) << "CellmlAdapterBase<nStates_,nAlgebraics_,FunctionSpaceType>::initialize querying meshManager for mesh";
    LOG(DEBUG) << "specificSettings_: ";
    PythonUtility::printDict(specificSettings_.pyObject());
  }
  
  // add this solver to the solvers diagram, which is a SVG file that will be created at the end of the simulation.
  DihuContext::solverStructureVisualizer()->addSolver("CellmlAdapter");

  // create a mesh if there is not yet one assigned, function space FunctionSpace::Generic
  if (!functionSpace_)
  {
    functionSpace_ = context_.meshManager()->functionSpace<FunctionSpaceType>(specificSettings_);  // create initialized mesh
  }
  LOG(DEBUG) << "Cellml mesh has " << functionSpace_->nNodesLocalWithoutGhosts() << " local nodes";

  //store number of instances
  nInstances_ = functionSpace_->nNodesLocalWithoutGhosts();


  // initialize source code generator
  std::string modelFilename = this->specificSettings_.getOptionString("modelFilename", "");
  if (this->specificSettings_.hasKey("sourceFilename"))
  {
    LOG(WARNING) << this->specificSettings_ << " Option \"sourceFilename\" has been renamed to \"modelFilename\".";
  }

  // initialize parameters
  std::vector<double> parametersInitialValues;
  specificSettings_.getOptionVector("parametersInitialValues", parametersInitialValues);

  // parse mappings, which initializes the following variables:
  // parametersUsedAsAlgebraic, parametersUsedAsConstant,

  // add explicitely defined parameters that replace algebraics and constants
  std::vector<int> parametersUsedAsAlgebraic;  //< explicitely defined parameters that will be copied to algebraics, this vector contains the indices of the algebraic array
  std::vector<int> parametersUsedAsConstant;      //< explicitely defined parameters that will be copied to constants, this vector contains the indices of the constants

  std::vector<int> &statesForTransfer = data_.statesForTransfer();
  std::vector<int> &algebraicsForTransfer = data_.algebraicsForTransfer();
  std::vector<int> &parametersForTransfer = data_.parametersForTransfer();

  // parse the source code and initialize the names of states, algebraics and constants, which are needed for initializeMappings
  cellmlSourceCodeGenerator_.initializeNames(modelFilename, nInstances_, nStates_, nAlgebraics_);

  // initialize all information from python settings key "mappings", this sets parametersUsedAsAlgebraics/States and outputAlgebraic/StatesIndex
  initializeMappings(parametersUsedAsAlgebraic, parametersUsedAsConstant,
                     statesForTransfer, algebraicsForTransfer, parametersForTransfer);

  // initialize data, i.e. states and algebraics field variables
  data_.setFunctionSpace(functionSpace_);
  data_.setAlgebraicNames(cellmlSourceCodeGenerator_.algebraicNames());
  data_.initialize();

  // get the data_.parameters() raw pointer
  data_.prepareParameterValues();

  // parse the source code completely and store source code, needs data initialized in order to store initial parameter values
  cellmlSourceCodeGenerator_.initializeSourceCode(parametersUsedAsAlgebraic, parametersUsedAsConstant, parametersInitialValues, nAlgebraics_, data_.parameterValues());

  // restore the raw pointer of data_.parameters()
  data_.restoreParameterValues();

  initializeStatesToEquilibrium_ = this->specificSettings_.getOptionBool("initializeStatesToEquilibrium", false);
  if (initializeStatesToEquilibrium_)
  {
    initializeStatesToEquilibriumTimestepWidth_ = this->specificSettings_.getOptionDouble("initializeStatesToEquilibriumTimestepWidth", 1e-4);
  }
}


template<int nStates_, int nAlgebraics_, typename FunctionSpaceType>
void CellmlAdapterBase<nStates_,nAlgebraics_,FunctionSpaceType>::
initializeMappings(std::vector<int> &parametersUsedAsAlgebraic, std::vector<int> &parametersUsedAsConstant,
                   std::vector<int> &statesForTransfer, std::vector<int> &algebraicsForTransfer, std::vector<int> &parametersForTransfer)
{
  if (this->specificSettings_.hasKey("mappings"))
  {
    // example for mappings:
    /*
    "mappings": {
      ("parameter",0): ("state",0),
      ("parameter",1): ("algebraic",0),
      ("parameter",2): ("constant",0),
      ("outputConnectorSlot",0): ("parameter",0),
    }
    */
    const std::vector<std::string> &algebraicNames = cellmlSourceCodeGenerator_.algebraicNames();
    const std::vector<std::string> &stateNames = cellmlSourceCodeGenerator_.stateNames();
    const std::vector<std::string> &constantNames = cellmlSourceCodeGenerator_.constantNames();


    // -----------------------------
    // parse all entries of "mappings" to tupleEntries of the following form
    /*
    "mappings": {
      ("parameter",0): ("state","membrane/V"),
      ("parameter",1): ("algebraic","razumova/activestress"),
      ("parameter",2): ("constant",0),
      ("outputConnectorSlot",0): ("parameter",0),
      ("outputConnectorSlot",1): "razumova/l_hs",       #<-- this will be converted now
    }
    */
    using EntryTypePython = std::pair<std::string,PyObject *>;
    std::vector<std::pair<EntryTypePython,EntryTypePython>> tupleEntries;

    std::pair<PyObject *,PyObject *> item = this->specificSettings_.template getOptionDictBegin<PyObject *,PyObject *>("mappings");

    for (;!this->specificSettings_.getOptionDictEnd("mappings");
         this->specificSettings_.template getOptionDictNext<PyObject *,PyObject *>("mappings",item))
    {
      EntryTypePython firstTuple;
      EntryTypePython secondTuple;

      if (PyTuple_Check(item.first))
      {
        firstTuple = PythonUtility::convertFromPython<EntryTypePython>::get(item.first);
      }
      else
      {
        std::string name = PythonUtility::convertFromPython<std::string>::get(item.first);

        // find if the string is a algebraic name given in algebraicNames
        std::vector<std::string>::const_iterator pos = std::find(algebraicNames.begin(), algebraicNames.end(), name);
        if (pos != algebraicNames.end())
        {
          firstTuple.first = "algebraic";
          firstTuple.second = PythonUtility::convertToPython<std::string>::get(name);
        }
        else
        {
          std::vector<std::string>::const_iterator pos = std::find(stateNames.begin(), stateNames.end(), name);
          if (pos != stateNames.end())
          {
            firstTuple.first = "state";
            firstTuple.second = PythonUtility::convertToPython<std::string>::get(name);
          }
          else
          {
            std::vector<std::string>::const_iterator pos = std::find(constantNames.begin(), constantNames.end(), name);
            if (pos != constantNames.end())
            {
              firstTuple.first = "constant";
              firstTuple.second = PythonUtility::convertToPython<std::string>::get(name);
            }
            else
            {
              LOG(ERROR) << "In mappings, name \"" << name << "\" is neither state, algebraic nor constant.";
            }
          }
        }
      }


      if (PyTuple_Check(item.second))
      {
        secondTuple = PythonUtility::convertFromPython<EntryTypePython>::get(item.second);
      }
      else
      {
        std::string name = PythonUtility::convertFromPython<std::string>::get(item.second);

        // find if the string is a algebraic name given in algebraicNames
        std::vector<std::string>::const_iterator pos = std::find(algebraicNames.begin(), algebraicNames.end(), name);
        if (pos != algebraicNames.end())
        {
          secondTuple.first = "algebraic";
          secondTuple.second = PythonUtility::convertToPython<std::string>::get(name);
        }
        else
        {
          std::vector<std::string>::const_iterator pos = std::find(stateNames.begin(), stateNames.end(), name);
          if (pos != stateNames.end())
          {
            secondTuple.first = "state";
            secondTuple.second = PythonUtility::convertToPython<std::string>::get(name);
          }
          else
          {
            std::vector<std::string>::const_iterator pos = std::find(constantNames.begin(), constantNames.end(), name);
            if (pos != constantNames.end())
            {
              secondTuple.first = "constant";
              secondTuple.second = PythonUtility::convertToPython<std::string>::get(name);
            }
            else
            {
              LOG(ERROR) << "In mappings, name \"" << name << "\" is neither state, algebraic nor constant.";
            }
          }
        }
      }

      tupleEntries.push_back(std::pair<EntryTypePython,EntryTypePython>(firstTuple, secondTuple));
    }

    // -----------------------------
    // parse the strings in the entries and convert them to stateNo, algebraicNo or constantNo
    /*
    "mappings": {
      ("parameter",0): ("state","membrane/V"),                      #<-- this will be converted now
      ("parameter",1): ("algebraic","razumova/activestress"),    #<-- this will be converted now
      ("parameter",2): ("constant",0),
      ("outputConnectorSlot",0): ("parameter",0),
      ("outputConnectorSlot",1): ("algebraic", "razumova/l_hs"), #<-- this will be converted now
    }
    */
    // result is settings for statesForTransfer, algebraicsForTransfer and parametersForTransfer

    for (std::pair<EntryTypePython,EntryTypePython> &tupleEntry : tupleEntries)
    {
      EntryTypePython entries[2] = {tupleEntry.first, tupleEntry.second};
      std::vector<std::pair<std::string,int>> entriesWithNos;

      // parse the strings in the entries and convert them to stateNo, algebraicNo or constantNo
      for (EntryTypePython entry : entries)
      {
        if (entry.first == "parameter" || entry.first == "outputConnectorSlot")
        {
          int parameterNo = PythonUtility::convertFromPython<int>::get(entry.second);
          entriesWithNos.push_back(std::make_pair(entry.first, parameterNo));
        }
        else if (entry.first == "state")
        {
          int stateNo = 0;
          std::string stateName = PythonUtility::convertFromPython<std::string>::get(entry.second);

          // if parsed string is a number, this is directly the state no
          if (!stateName.empty() && stateName.find_first_not_of("0123456789") == std::string::npos)
          {
            stateNo = atoi(stateName.c_str());
          }
          else
          {
            // find if the string is a state name given in stateNames
            const std::vector<std::string>::const_iterator pos = std::find(stateNames.begin(), stateNames.end(), stateName);
            if (pos != stateNames.end())
            {
              stateNo = std::distance(stateNames.begin(),pos);
            }
            else
            {
              LOG(ERROR) << "In " << this->specificSettings_ << "[\"mappings\"], state \"" << stateName << "\" is not valid. "
                << "Valid state names are: " << stateNames << ".\n"
                << "You can also specify the state no. instead of the name.";
            }
          }

          entriesWithNos.push_back(std::make_pair(entry.first, stateNo));
        }
        else if (entry.first == "algebraic" || entry.first == "intermediate")
        {
          int algebraicNo = 0;
          std::string algebraicName = PythonUtility::convertFromPython<std::string>::get(entry.second);

          // if parsed string is a number, this is directly the algebraic no
          if (!algebraicName.empty() && algebraicName.find_first_not_of("0123456789") == std::string::npos)
          {
            algebraicNo = atoi(algebraicName.c_str());
          }
          else
          {
            // find if the string is a algebraic name given in algebraicNames
            std::vector<std::string>::const_iterator pos = std::find(algebraicNames.begin(), algebraicNames.end(), algebraicName);
            if (pos != algebraicNames.end())
            {
              algebraicNo = std::distance(algebraicNames.begin(),pos);
            }
            else
            {
              LOG(ERROR) << "In " << this->specificSettings_ << "[\"mappings\"], algebraic \"" << algebraicName << "\" is not valid. "
                << "Valid algebraic names are: " << algebraicNames << ".\n"
                << "You can also specify the algebraic no. instead of the name.";
            }
          }

          entriesWithNos.push_back(std::make_pair(entry.first, algebraicNo));
        }
        else if (entry.first == "constant")
        {
          int constantNo = 0;
          std::string constantName = PythonUtility::convertFromPython<std::string>::get(entry.second);

          // if parsed string is a number, this is directly the constant no
          if (!constantName.empty() && constantName.find_first_not_of("0123456789") == std::string::npos)
          {
            constantNo = atoi(constantName.c_str());
          }
          else
          {
            // find if the string is a constant name given in constantNames
            std::vector<std::string>::const_iterator pos = std::find(constantNames.begin(), constantNames.end(), constantName);
            if (pos != constantNames.end())
            {
              constantNo = std::distance(constantNames.begin(),pos);
            }
            else
            {
              LOG(ERROR) << "In " << this->specificSettings_ << "[\"mappings\"], constant \"" << constantName << "\" is not valid. "
                << "Valid constant names are: " << constantNames << ".\n"
                << "You can also specify the constant no. instead of the name.";
            }
          }

          entriesWithNos.push_back(std::make_pair(entry.first, constantNo));
        }
        else
        {
          LOG(FATAL) << "In " << this->specificSettings_ << "[\"mappings\"], invalid field \"" << entry.first << "\", allowed values are: "
            << "\"parameter\", \"outputConnectorSlot\", \"state\", \"algebraic\", \"constant\".";
        }
      }

      LOG(DEBUG) << "parsed next item, entriesWithNos: " << entriesWithNos;

      // make sure that the first part is "paramater" or "outputConnectorSlot"
      if (entriesWithNos[1].first == "parameter" || entriesWithNos[1].first == "outputConnectorSlot")
      {
        std::swap(entriesWithNos[0], entriesWithNos[1]);
      }

      // handle parameter
      if (entriesWithNos[0].first == "parameter")
      {
        int parameterNo = entriesWithNos[0].second;    // the no. of the parameter
        int fieldNo = entriesWithNos[1].second;        // the no. of the state, algebraic or constant

        LOG(DEBUG) << "parameter " << parameterNo << " to \"" << entriesWithNos[1].first << "\", fieldNo " << fieldNo;

        if (entriesWithNos[1].first == "algebraic" || entriesWithNos[1].first == "algebraic")
        {
          // add no of the constant to parametersUsedAsAlgebraic
          if (parametersUsedAsAlgebraic.size() <= parameterNo)
          {
            parametersUsedAsAlgebraic.resize(parameterNo+1, -1);
          }
          parametersUsedAsAlgebraic[parameterNo] = fieldNo;
        }
        else if (entriesWithNos[1].first == "constant")
        {
          // add no of the constant to parametersUsedAsConstant
          if (parametersUsedAsConstant.size() <= parameterNo)
          {
            parametersUsedAsConstant.resize(parameterNo+1, -1);
          }
          parametersUsedAsConstant[parameterNo] = fieldNo;
        }
        else
        {
          LOG(ERROR) << "In " << this->specificSettings_ << "[\"mappings\"], you can map "
            << "paramaters only to algebraics and constants, not states.";
        }
      }
      else
      {
        // output connector slots

        int slotNo = entriesWithNos[0].second;    // the no. of the output connector slot
        int fieldNo = entriesWithNos[1].second;        // the no. of the state, algebraic or constant

        LOG(DEBUG) << "output connector slot " << slotNo << " to \"" << entriesWithNos[1].first << "\", fieldNo " << fieldNo;

        if (entriesWithNos[1].first == "state")
        {
          // add no of the constant to statesForTransfer
          if (statesForTransfer.size() <= slotNo)
          {
            statesForTransfer.resize(slotNo+1, -1);
          }
          statesForTransfer[slotNo] = fieldNo;
        }
        else if (entriesWithNos[1].first == "algebraic" || entriesWithNos[1].first == "intermediate")
        {
          // add no of the constant to algebraicsForTransfer
          if (algebraicsForTransfer.size() <= slotNo)
          {
            algebraicsForTransfer.resize(slotNo+1, -1);
          }
          algebraicsForTransfer[slotNo] = fieldNo;
        }
        else if (entriesWithNos[1].first == "parameter")
        {
          // add no of the constant to parametersForTransfer
          if (parametersForTransfer.size() <= slotNo)
          {
            parametersForTransfer.resize(slotNo+1, -1);
          }
          parametersForTransfer[slotNo] = fieldNo;
        }
        else
        {
          LOG(ERROR) << "In " << this->specificSettings_ << "[\"mappings\"], you can map "
            << "outputConnectorSlots only to states, algebraics and parameters, not constants.";
        }
      }
    }

    // reorder slot nos such that states go first, then algebraics, then parameters
    std::vector<int> temporaryVector;
    std::copy_if(statesForTransfer.begin(), statesForTransfer.end(),
                 std::back_insert_iterator<std::vector<int>>(temporaryVector), [](int a){return a != -1;});
    statesForTransfer = temporaryVector;

    temporaryVector.clear();
    std::copy_if(algebraicsForTransfer.begin(), algebraicsForTransfer.end(),
                 std::back_insert_iterator<std::vector<int>>(temporaryVector), [](int a){return a != -1;});
    algebraicsForTransfer = temporaryVector;

    temporaryVector.clear();
    std::copy_if(parametersForTransfer.begin(), parametersForTransfer.end(),
                 std::back_insert_iterator<std::vector<int>>(temporaryVector), [](int a){return a != -1;});
    parametersForTransfer = temporaryVector;

    // create a string of parameter and contstant mappings
    std::stringstream s;
    for (int i = 0; i < parametersUsedAsAlgebraic.size(); i++)
    {
      int algebraicNo = parametersUsedAsAlgebraic[i];
      if (algebraicNo != -1 && algebraicNo < nAlgebraics_)
      {
        s << "  parameter " << i << " maps to algebraic " << algebraicNo << " (\"" << algebraicNames[algebraicNo] << "\")\n";
      }
    }
    for (int i = 0; i < parametersUsedAsConstant.size(); i++)
    {
      int constantNo = parametersUsedAsConstant[i];
      if (constantNo != -1 && constantNo < constantNames.size())
      {
        s << "  parameter " << i << " maps to constant " << constantNo << " (\"" << constantNames[constantNo] << "\")\n";
      }
    }

    // check that parameters are all mapped
    for (int i = 0; i < parametersUsedAsAlgebraic.size(); i++)
    {
      int algebraicNo = parametersUsedAsAlgebraic[i];
      if (algebraicNo == -1 && algebraicNo < nAlgebraics_)
      {
        LOG(ERROR) << "Parameter no. " << i << " is not mapped to any algebraic. "
          << "(Note that parameters have to be ordered in a way that the first are mapped to algebraics and the last to constants.)\n"
          << " Parsed mapping:\n" << s.str();
      }
    }

    for (int i = 0; i < parametersUsedAsConstant.size(); i++)
    {
      int constantNo = parametersUsedAsConstant[i];
      if (constantNo == -1 && constantNo < constantNames.size())
      {
        LOG(ERROR) << "Parameter no. " << parametersUsedAsAlgebraic.size() + i << " is not mapped to any constant. "
          << "(Note that parameters have to be ordered in a way that the first are mapped to algebraics and the last to constants.)\n"
          << " Parsed mapping:\n" << s.str();
      }
    }
  }
  else
  {
    this->specificSettings_.getOptionVector("parametersUsedAsAlgebraic", parametersUsedAsAlgebraic);
    this->specificSettings_.getOptionVector("parametersUsedAsConstant", parametersUsedAsConstant);

    this->specificSettings_.template getOptionVector<int>("statesForTransfer", statesForTransfer);
    this->specificSettings_.template getOptionVector<int>("algebraicsForTransfer", algebraicsForTransfer);
    this->specificSettings_.template getOptionVector<int>("parametersForTransfer", parametersForTransfer);
  }

  LOG(DEBUG) << "parametersUsedAsAlgebraic: " << parametersUsedAsAlgebraic;
  LOG(DEBUG) << "parametersUsedAsConstant:  " << parametersUsedAsConstant;
  LOG(DEBUG) << "statesForTransfer:         " << statesForTransfer;
  LOG(DEBUG) << "algebraicsForTransfer:     " << algebraicsForTransfer;
  LOG(DEBUG) << "parametersForTransfer:     " << parametersForTransfer;

  if (this->specificSettings_.hasKey("outputAlgebraicIndex"))
  {
    LOG(WARNING) << specificSettings_ << "[\"outputAlgebraicIndex\"] is no longer a valid option, use \"algebraicsForTransfer\" instead!";
  }

  if (this->specificSettings_.hasKey("outputStateIndex"))
  {
    LOG(WARNING) << specificSettings_ << "[\"outputStateIndex\"] is no longer a valid option, use \"statesForTransfer\" instead!";
  }
}

template<int nStates_, int nAlgebraics_, typename FunctionSpaceType>
template<typename FunctionSpaceType2>
bool CellmlAdapterBase<nStates_,nAlgebraics_,FunctionSpaceType>::
setInitialValues(std::shared_ptr<FieldVariable::FieldVariable<FunctionSpaceType2,nStates_>> initialValues)
{
  LOG(TRACE) << "CellmlAdapterBase<nStates_,nAlgebraics_,FunctionSpaceType>::setInitialValues";

  if (!statesInitialValuesinitialized_)
  {

    // initialize states
    if (this->specificSettings_.hasKey("statesInitialValues") && !this->specificSettings_.isEmpty("statesInitialValues"))
    {
      LOG(DEBUG) << "set initial values from config";

      // statesInitialValues gives the initial state values for one instance of the problem. it is used for all instances.
      std::array<double,nStates_> statesInitialValuesFromConfig = this->specificSettings_.template getOptionArray<double,nStates_>("statesInitialValues", 0);

      // store initial values to statesInitialValues_
      std::copy(statesInitialValuesFromConfig.begin(), statesInitialValuesFromConfig.end(), statesInitialValues_.begin());
    }
    else
    {
      LOG(DEBUG) << "set initial values from source file";

      // parsing the source file was already done
      // get initial values from source code generator
      std::vector<double> statesInitialValuesGenerator = cellmlSourceCodeGenerator_.statesInitialValues();
      assert(statesInitialValuesGenerator.size() == nStates_);

      std::copy(statesInitialValuesGenerator.begin(), statesInitialValuesGenerator.end(), statesInitialValues_.begin());
    }

    if (initializeStatesToEquilibrium_)
    {
      this->initializeToEquilibriumValues(statesInitialValues_);
    }
    statesInitialValuesinitialized_ = true;
  }

  // Here we have the initial values for the states in the statesInitialValues vector, only for one instance.
  VLOG(1) << "statesInitialValues: " << statesInitialValues_;
  const std::vector<std::array<double,nStates_>> statesAllInstances(nInstances_, statesInitialValues_);

  VLOG(1) << "statesAllInstances: " << statesAllInstances << ", nInstances: " << nInstances_ << ", nStates_ per instances: " << statesInitialValues_.size();
  initialValues->setValuesWithoutGhosts(statesAllInstances);

  VLOG(1) << "initialValues: " << *initialValues;
  return true;
}

template<int nStates_, int nAlgebraics_, typename FunctionSpaceType>
std::shared_ptr<FunctionSpaceType> CellmlAdapterBase<nStates_,nAlgebraics_,FunctionSpaceType>::
functionSpace()
{
  return functionSpace_;
}

template<int nStates_, int nAlgebraics_, typename FunctionSpaceType>
void CellmlAdapterBase<nStates_,nAlgebraics_,FunctionSpaceType>::
getNumbers(int& nInstances, int& nAlgebraics, int& nParameters)
{
  nInstances = nInstances_;
  nAlgebraics = nAlgebraics_;
  nParameters = cellmlSourceCodeGenerator_.nParameters();
}

template<int nStates_, int nAlgebraics_, typename FunctionSpaceType>
std::vector<int> &CellmlAdapterBase<nStates_,nAlgebraics_,FunctionSpaceType>::
statesForTransfer()
{
  return data_.statesForTransfer();
}

template<int nStates_, int nAlgebraics_, typename FunctionSpaceType>
std::vector<int> &CellmlAdapterBase<nStates_,nAlgebraics_,FunctionSpaceType>::
algebraicsForTransfer()
{
  return data_.algebraicsForTransfer();
}

template<int nStates_, int nAlgebraics_, typename FunctionSpaceType>
void CellmlAdapterBase<nStates_,nAlgebraics_,FunctionSpaceType>::
getStateNames(std::vector<std::string> &stateNames)
{
  stateNames = this->cellmlSourceCodeGenerator_.stateNames();
}

template<int nStates_, int nAlgebraics_, typename FunctionSpaceType>
typename CellmlAdapterBase<nStates_,nAlgebraics_,FunctionSpaceType>::Data &CellmlAdapterBase<nStates_,nAlgebraics_,FunctionSpaceType>::
data()
{
  return this->data_;
}

template<int nStates_, int nAlgebraics_, typename FunctionSpaceType>
CellmlSourceCodeGenerator &CellmlAdapterBase<nStates_,nAlgebraics_,FunctionSpaceType>::
cellmlSourceCodeGenerator()
{
  return this->cellmlSourceCodeGenerator_;
}

template<int nStates_, int nAlgebraics_, typename FunctionSpaceType>
PythonConfig CellmlAdapterBase<nStates_,nAlgebraics_,FunctionSpaceType>::
specificSettings()
{
  return this->specificSettings_;
}

template<int nStates_, int nAlgebraics_, typename FunctionSpaceType>
constexpr int  CellmlAdapterBase<nStates_,nAlgebraics_,FunctionSpaceType>::
nAlgebraics() const
{
  return nAlgebraics_;
}

template<int nStates_, int nAlgebraics_, typename FunctionSpaceType>
std::shared_ptr<typename CellmlAdapterBase<nStates_,nAlgebraics_,FunctionSpaceType>::OutputConnectorDataType>
CellmlAdapterBase<nStates_,nAlgebraics_,FunctionSpaceType>::
getOutputConnectorData()
{
  return this->data_.getOutputConnectorData();
}
