#include <iostream>
#include <cstdlib>

#include "opendihu.h"

int main(int argc, char *argv[])
{
  // 1D Laplace equation 0 = du^2/dx^2
  
  // initialize everything, handle arguments and parse settings from input file
  DihuContext settings(argc, argv);
  
  SpatialDiscretization::FiniteElementMethod<
    Mesh::Regular<1>,
    BasisFunction::Lagrange<1>,
    Equation::Static::Laplace
  > equationDiscretized(settings);
  
  Computation computation(settings, equationDiscretized);
  computation.run();
  
  return EXIT_SUCCESS;
}