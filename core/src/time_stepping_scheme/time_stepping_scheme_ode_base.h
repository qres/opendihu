#pragma once

#include "control/dihu_context.h"
#include "data_management/time_stepping/time_stepping.h"
#include "interfaces/discretizable_in_time.h"
#include "time_stepping_scheme/time_stepping_scheme.h"
#include "data_management/data.h"
#include "data_management/time_stepping/time_stepping.h"
#include "cellml/03_cellml_adapter.h"

namespace TimeSteppingScheme
{


/**
  * Normal timestepping scheme to solve ODES, like Heun
  */
template<typename FunctionSpaceType, int nComponents>
class TimeSteppingSchemeOdeBase :
public TimeSteppingScheme
{
public:
  typedef FunctionSpaceType FunctionSpace;
  typedef typename Data::TimeStepping<FunctionSpaceType, nComponents> Data;   // type of Data object

  //! constructor
  TimeSteppingSchemeOdeBase(DihuContext context, std::string name);

  //! destructor
  virtual ~TimeSteppingSchemeOdeBase() {}

  //! run simulation
  virtual void run();

  //! return the data object
  Data &data();

  //! output the given data for debugging
  //virtual std::string getString(OutputConnectorDataType &data);

  //! initialize discretizableInTime
  virtual void initialize();

  //! set the subset of ranks that will compute the work
  void setRankSubset(Partition::RankSubset rankSubset);

  //! reset state such that new initialization becomes necessary
  virtual void reset();

protected:

  //! read initial values from settings and set field accordingly
  void setInitialValues();

  std::shared_ptr<Data> data_;     ///< data object that holds all PETSc vectors and matrices

  bool initialized_;     ///< if initialize() was already called

  double prefactor_;     ///< a factor with which the result is multiplied when the data is used in a splitting scheme

};

}  // namespace

#include "time_stepping_scheme/time_stepping_scheme_ode_base.tpp"
