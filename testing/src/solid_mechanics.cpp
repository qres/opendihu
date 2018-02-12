#include <Python.h>  // this has to be the first included header

#include <iostream>
#include <cstdlib>
#include <fstream>
#include <cassert>

#include "gtest/gtest.h"
#include "opendihu.h"
#include "utility.h"
#include "arg.h"
#include "stiffness_matrix_tester.h"
#include "node_positions_tester.h"

TEST(SolidMechanicsTest, Test0)
{
  std::string pythonConfig = R"(
# solid mechanics (3D)

config = {
  "disablePrinting": False,
  "disableMatrixPrinting": False,
  "FiniteElementMethod" : {
    "nElements": [2,2,2],   # 8 elements
    "physicalExtend": [4.0,4.0,4.0],
    "DirichletBoundaryCondition": {0:1.0},
    "relativeTolerance": 1e-15,
    "OutputWriter" : [
      {"format": "PythonFile", "filename" : "out_txt", "binary" : False},
    ]
  }
}
)";
  DihuContext settings(argc, argv, pythonConfig);
  
  SpatialDiscretization::FiniteElementMethod<
    Mesh::StructuredDeformable<3>,
    BasisFunction::Mixed<
      BasisFunction::Lagrange<1>,
      BasisFunction::Lagrange<2>
    >,
    Integrator::Mixed<
      Integrator::Gauss<2>,
      Integrator::Gauss<3>
    >,
    Equation::Static::SolidMechanics
  > equationDiscretized(settings);
  
  Computation computation(settings, equationDiscretized);
  computation.run();
}


