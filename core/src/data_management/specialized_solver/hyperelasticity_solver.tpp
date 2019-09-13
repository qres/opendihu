#include "data_management/specialized_solver/hyperelasticity_solver.h"

namespace Data
{

template<typename PressureFunctionSpace, typename DisplacementsFunctionSpace, typename Term>
  QuasiStaticHyperelasticityBase<PressureFunctionSpace,DisplacementsFunctionSpace,Term>::
QuasiStaticHyperelasticityBase(DihuContext context) :
  Data<DisplacementsFunctionSpace>::Data(context)
{
}

template<typename PressureFunctionSpace, typename DisplacementsFunctionSpace, typename Term>
void QuasiStaticHyperelasticityBase<PressureFunctionSpace,DisplacementsFunctionSpace,Term>::
initialize()
{
  // call initialize of base class, this calls createPetscObjects
  Data<DisplacementsFunctionSpace>::initialize();
}

template<typename PressureFunctionSpace, typename DisplacementsFunctionSpace, typename Term>
void QuasiStaticHyperelasticityBase<PressureFunctionSpace,DisplacementsFunctionSpace,Term>::
createPetscObjects()
{
  LOG(DEBUG) << "QuasiStaticHyperelasticityBase::createPetscObject";

  assert(this->displacementsFunctionSpace_);
  assert(this->pressureFunctionSpace_);
  assert(this->functionSpace_);


  std::vector<std::string> displacementsComponentNames({"x","y","z"});
  displacements_           = this->displacementsFunctionSpace_->template createFieldVariable<3>("u", displacementsComponentNames);     //< u, the displacements
  fiberDirection_          = this->displacementsFunctionSpace_->template createFieldVariable<3>("fiberDirection", displacementsComponentNames);
  displacementsLinearMesh_ = this->pressureFunctionSpace_->template createFieldVariable<3>("uLin", displacementsComponentNames);     //< u, the displacements
  pressure_                = this->pressureFunctionSpace_->template createFieldVariable<1>("p");     //<  p, the pressure variable

  std::vector<std::string> componentNames({"S_11", "S_22", "S_33", "S_12", "S_13", "S_23"});
  pK2Stress_               = this->displacementsFunctionSpace_->template createFieldVariable<6>("PK2-Stress (Voigt)", componentNames);     //<  the symmetric PK2 stress tensor in Voigt notation
}


//! field variable of geometryReference_
template<typename PressureFunctionSpace, typename DisplacementsFunctionSpace, typename Term>
std::shared_ptr<typename QuasiStaticHyperelasticityBase<PressureFunctionSpace,DisplacementsFunctionSpace,Term>::DisplacementsFieldVariableType> QuasiStaticHyperelasticityBase<PressureFunctionSpace,DisplacementsFunctionSpace,Term>::
geometryReference()
{
  return this->geometryReference_;
}

//! field variable of u
template<typename PressureFunctionSpace, typename DisplacementsFunctionSpace, typename Term>
std::shared_ptr<typename QuasiStaticHyperelasticityBase<PressureFunctionSpace,DisplacementsFunctionSpace,Term>::DisplacementsFieldVariableType> QuasiStaticHyperelasticityBase<PressureFunctionSpace,DisplacementsFunctionSpace,Term>::
displacements()
{
  return this->displacements_;
}

//! field variable of fiber direction
template<typename PressureFunctionSpace, typename DisplacementsFunctionSpace, typename Term>
std::shared_ptr<typename QuasiStaticHyperelasticityBase<PressureFunctionSpace,DisplacementsFunctionSpace,Term>::DisplacementsFieldVariableType> QuasiStaticHyperelasticityBase<PressureFunctionSpace,DisplacementsFunctionSpace,Term>::
fiberDirection()
{
  return this->fiberDirection_;
}

//! field variable displacements u but on the linear mesh
template<typename PressureFunctionSpace, typename DisplacementsFunctionSpace, typename Term>
std::shared_ptr<typename QuasiStaticHyperelasticityBase<PressureFunctionSpace,DisplacementsFunctionSpace,Term>::DisplacementsLinearFieldVariableType> QuasiStaticHyperelasticityBase<PressureFunctionSpace,DisplacementsFunctionSpace,Term>::
displacementsLinearMesh()
{
  return this->displacementsLinearMesh_;
}

//! field variable of p
template<typename PressureFunctionSpace, typename DisplacementsFunctionSpace, typename Term>
std::shared_ptr<typename QuasiStaticHyperelasticityBase<PressureFunctionSpace,DisplacementsFunctionSpace,Term>::PressureFieldVariableType> QuasiStaticHyperelasticityBase<PressureFunctionSpace,DisplacementsFunctionSpace,Term>::
pressure()
{
  return this->pressure_;
}

//! field variable of S
template<typename PressureFunctionSpace, typename DisplacementsFunctionSpace, typename Term>
std::shared_ptr<typename QuasiStaticHyperelasticityBase<PressureFunctionSpace,DisplacementsFunctionSpace,Term>::StressFieldVariableType> QuasiStaticHyperelasticityBase<PressureFunctionSpace,DisplacementsFunctionSpace,Term>::
pK2Stress()
{
  return this->pK2Stress_;
}

template<typename PressureFunctionSpace, typename DisplacementsFunctionSpace, typename Term>
void QuasiStaticHyperelasticityBase<PressureFunctionSpace,DisplacementsFunctionSpace,Term>::
updateGeometry(double scalingFactor)
{
  PetscErrorCode ierr;

  this->displacementsFunctionSpace_->geometryField().finishGhostManipulation();

  // update quadratic function space geometry
  // w = alpha * x + y, VecWAXPY(w, alpha, x, y)
  ierr = VecWAXPY(this->displacementsFunctionSpace_->geometryField().valuesGlobal(),
                  scalingFactor, this->displacements_->valuesGlobal(), this->geometryReference_->valuesGlobal()); CHKERRV(ierr);

  this->displacementsFunctionSpace_->geometryField().startGhostManipulation();

  // for displacements extract linear mesh from quadratic mesh
  std::vector<Vec3> displacementValues;
  this->displacements_->getValuesWithGhosts(displacementValues);

  node_no_t nNodesLocal[3] = {
    this->displacementsFunctionSpace_->meshPartition()->nNodesLocalWithGhosts(0),
    this->displacementsFunctionSpace_->meshPartition()->nNodesLocalWithGhosts(1),
    this->displacementsFunctionSpace_->meshPartition()->nNodesLocalWithGhosts(2)
  };

  std::vector<Vec3> linearMeshDisplacementValues(this->pressureFunctionSpace_->meshPartition()->nNodesLocalWithGhosts());
  int linearMeshIndex = 0;

  // loop over linear nodes in the quadratic mesh
  for (int k = 0; k < nNodesLocal[2]; k+=2)
  {
    for (int j = 0; j < nNodesLocal[1]; j+=2)
    {
      for (int i = 0; i < nNodesLocal[0]; i+=2, linearMeshIndex++)
      {
        int index = k*nNodesLocal[0]*nNodesLocal[1] + j*nNodesLocal[0] + i;

        linearMeshDisplacementValues[linearMeshIndex] = displacementValues[index];
      }
    }
  }

  displacementsLinearMesh_->setValuesWithGhosts(linearMeshDisplacementValues, INSERT_VALUES);

  // update linear function space geometry
  this->pressureFunctionSpace_->geometryField().finishGhostManipulation();

  // w = alpha * x + y, VecWAXPY(w, alpha, x, y)
  ierr = VecWAXPY(this->pressureFunctionSpace_->geometryField().valuesGlobal(),
                  1, this->displacementsLinearMesh_->valuesGlobal(), this->geometryReferenceLinearMesh_->valuesGlobal()); CHKERRV(ierr);

  this->pressureFunctionSpace_->geometryField().startGhostManipulation();
}

//! set the function space object that discretizes the pressure field variable
template<typename PressureFunctionSpace, typename DisplacementsFunctionSpace, typename Term>
void QuasiStaticHyperelasticityBase<PressureFunctionSpace,DisplacementsFunctionSpace,Term>::
setPressureFunctionSpace(std::shared_ptr<PressureFunctionSpace> pressureFunctionSpace)
{
  pressureFunctionSpace_ = pressureFunctionSpace;

  // set the geometry field of the reference configuration as copy of the geometry field of the function space
  geometryReferenceLinearMesh_ = std::make_shared<DisplacementsLinearFieldVariableType>(pressureFunctionSpace_->geometryField(), "geometryReferenceLinearMesh");
  geometryReferenceLinearMesh_->setValues(pressureFunctionSpace_->geometryField());
}

//! set the function space object that discretizes the displacements field variable
template<typename PressureFunctionSpace, typename DisplacementsFunctionSpace, typename Term>
void QuasiStaticHyperelasticityBase<PressureFunctionSpace,DisplacementsFunctionSpace,Term>::
setDisplacementsFunctionSpace(std::shared_ptr<DisplacementsFunctionSpace> displacementsFunctionSpace)
{
  displacementsFunctionSpace_ = displacementsFunctionSpace;

  // also set the functionSpace_ variable which is from the parent class Data
  this->functionSpace_ = displacementsFunctionSpace;

  LOG(DEBUG) << "create geometry Reference";

  // set the geometry field of the reference configuration as copy of the geometry field of the function space
  geometryReference_ = std::make_shared<DisplacementsFieldVariableType>(displacementsFunctionSpace_->geometryField(), "geometryReference");
  geometryReference_->setValues(displacementsFunctionSpace_->geometryField());
}

//! get the displacements function space
template<typename PressureFunctionSpace, typename DisplacementsFunctionSpace, typename Term>
std::shared_ptr<DisplacementsFunctionSpace> QuasiStaticHyperelasticityBase<PressureFunctionSpace,DisplacementsFunctionSpace,Term>::
displacementsFunctionSpace()
{
  return displacementsFunctionSpace_;
}

//! get the pressure function space
template<typename PressureFunctionSpace, typename DisplacementsFunctionSpace, typename Term>
std::shared_ptr<PressureFunctionSpace> QuasiStaticHyperelasticityBase<PressureFunctionSpace,DisplacementsFunctionSpace,Term>::
pressureFunctionSpace()
{
  return pressureFunctionSpace_;
}

template<typename PressureFunctionSpace, typename DisplacementsFunctionSpace, typename Term>
void QuasiStaticHyperelasticityBase<PressureFunctionSpace,DisplacementsFunctionSpace,Term>::
print()
{
}


template<typename PressureFunctionSpace, typename DisplacementsFunctionSpace, typename Term>
typename QuasiStaticHyperelasticity<PressureFunctionSpace, DisplacementsFunctionSpace, Term, std::enable_if_t<Term::usesFiberDirection,Term>>::FieldVariablesForOutputWriter
QuasiStaticHyperelasticity<PressureFunctionSpace, DisplacementsFunctionSpace, Term, std::enable_if_t<Term::usesFiberDirection,Term>>::
getFieldVariablesForOutputWriter()
{
  LOG(DEBUG) << "getFieldVariablesForOutputWriter, with fiberDirection";
  return std::make_tuple(
    std::shared_ptr<DisplacementsFieldVariableType>(std::make_shared<typename DisplacementsFunctionSpace::GeometryFieldType>(this->displacementsFunctionSpace_->geometryField())), // geometry
    std::shared_ptr<DisplacementsFieldVariableType>(this->displacements_),              // displacements_
    std::shared_ptr<StressFieldVariableType>(this->pK2Stress_),         // pK2Stress_
    std::shared_ptr<DisplacementsFieldVariableType>(this->fiberDirection_)
  );
}

template<typename PressureFunctionSpace, typename DisplacementsFunctionSpace, typename Term, typename DummyForTraits>
typename QuasiStaticHyperelasticity<PressureFunctionSpace,DisplacementsFunctionSpace,Term,DummyForTraits>::FieldVariablesForOutputWriter QuasiStaticHyperelasticity<PressureFunctionSpace,DisplacementsFunctionSpace,Term,DummyForTraits>::
getFieldVariablesForOutputWriter()
{
  LOG(DEBUG) << "getFieldVariablesForOutputWriter, without fiberDirection";

  // these field variables will be written to output files
  return std::make_tuple(
    std::shared_ptr<DisplacementsFieldVariableType>(std::make_shared<typename DisplacementsFunctionSpace::GeometryFieldType>(this->displacementsFunctionSpace_->geometryField())), // geometry
    std::shared_ptr<DisplacementsFieldVariableType>(this->displacements_),              // displacements_
    std::shared_ptr<StressFieldVariableType>(this->pK2Stress_)         // pK2Stress_
  );

  /*
  // code to output the pressure field variables
  return std::tuple_cat(
    std::tuple<std::shared_ptr<DisplacementsLinearFieldVariableType>>(std::make_shared<typename PressureFunctionSpace::GeometryFieldType>(this->pressureFunctionSpace_->geometryField())), // geometry
    std::tuple<std::shared_ptr<PressureFieldVariableType>>(this->pressure_)
  );
  */
}

// --------------------------------
// QuasiStaticHyperelasticityPressureOutput

template<typename PressureFunctionSpace>
void QuasiStaticHyperelasticityPressureOutput<PressureFunctionSpace>::
initialize(std::shared_ptr<typename QuasiStaticHyperelasticityPressureOutput<PressureFunctionSpace>::PressureFieldVariableType> pressure,
           std::shared_ptr<typename QuasiStaticHyperelasticityPressureOutput<PressureFunctionSpace>::DisplacementsLinearFieldVariableType> displacementsLinearMesh)
{
  pressure_ = pressure;
  displacementsLinearMesh_ = displacementsLinearMesh;
}

template<typename PressureFunctionSpace>
typename QuasiStaticHyperelasticityPressureOutput<PressureFunctionSpace>::FieldVariablesForOutputWriter QuasiStaticHyperelasticityPressureOutput<PressureFunctionSpace>::
getFieldVariablesForOutputWriter()
{
  // these field variables will be written to output files
  return std::tuple_cat(
    std::tuple<std::shared_ptr<DisplacementsLinearFieldVariableType>>(std::make_shared<typename PressureFunctionSpace::GeometryFieldType>(this->functionSpace_->geometryField())), // geometry
    std::tuple<std::shared_ptr<DisplacementsLinearFieldVariableType>>(this->displacementsLinearMesh_),
    std::tuple<std::shared_ptr<PressureFieldVariableType>>(this->pressure_)
  );
}

} // namespace
