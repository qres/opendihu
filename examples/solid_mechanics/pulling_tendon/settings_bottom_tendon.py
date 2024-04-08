# Transversely-isotropic Mooney Rivlin on a tendon geometry
# Note, this is not possible to be run in parallel because the fibers cannot be initialized without MultipleInstances class.
import sys, os
import numpy as np
import sys

# parse rank arguments
rank_no = (int)(sys.argv[-2])
n_ranks = (int)(sys.argv[-1])

#add variables subfolder to python path where the variables script is located
script_path = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, script_path)
sys.path.insert(0, os.path.join(script_path,'variables'))

import variables              # file variables.py, defines default values for all parameters, you can set the parameters there

variables.scenario_name = "bottom_tendon"

# compute partitioning
if rank_no == 0:
  if n_ranks != variables.n_subdomains_x*variables.n_subdomains_y*variables.n_subdomains_z:
    print("\n\nError! Number of ranks {} does not match given partitioning {} x {} x {} = {}.\n\n".format(n_ranks, variables.n_subdomains_x, variables.n_subdomains_y, variables.n_subdomains_z, variables.n_subdomains_x*variables.n_subdomains_y*variables.n_subdomains_z))
    sys.exit(-1)
    
# update material parameters
if (variables.tendon_material == "nonLinear"):
    c = 9.98                    # [N/cm^2=kPa]
    ca = 14.92                  # [-]
    ct = 14.7                   # [-]
    cat = 9.64                  # [-]
    ctt = 11.24                 # [-]
    mu = 3.76                   # [N/cm^2=kPa]
    k1 = 42.217e3               # [N/cm^2=kPa]
    k2 = 411.360e3              # [N/cm^2=kPa]

    variables.material_parameters = [c, ca, ct, cat, ctt, mu, k1, k2]

if (variables.tendon_material == "SaintVenantKirchoff"):
    # material parameters for Saint Venant-Kirchhoff material
    # https://www.researchgate.net/publication/230248067_Bulk_Modulus

    youngs_modulus = 7e4        # [N/cm^2 = 10kPa]  
    shear_modulus = 3e4

    lambd = shear_modulus*(youngs_modulus - 2*shear_modulus) / (3*shear_modulus - youngs_modulus)  # Lamé parameter lambda
    mu = shear_modulus       # Lamé parameter mu or G (shear modulus)

    variables.material_parameters = [lambd, mu]


# add meshes
meshes_tendon = {
  "tendon_Mesh": {
    "nElements" :         variables.bottom_n_elements_tendon,
    "physicalExtent":     variables.bottom_tendon_extent,
    "physicalOffset":     variables.bottom_tendon_offset,
    "logKey":             "tendon",
    "inputMeshIsGlobal":  True,
    "nRanks":             n_ranks
  },
  "tendon_Mesh_quadratic": {
    "nElements" :         [elems // 2 for elems in variables.bottom_n_elements_tendon],
    "physicalExtent":     variables.bottom_tendon_extent,
    "physicalOffset":     variables.bottom_tendon_offset,
    "logKey":             "tendon_quadratic",
    "inputMeshIsGlobal":  True,
    "nRanks":             n_ranks,
  }
}

# boundary conditions (for quadratic elements)
# --------------------------------------------

[nx, ny, nz] = [elem + 1 for elem in variables.bottom_n_elements_tendon]
[mx, my, mz] = [elem // 2 for elem in variables.bottom_n_elements_tendon] # quadratic elements consist of 2 linear elements along each axis

# Neumann: Pull the bottom of the tendon
k = 0

variables.elasticity_neumann_bc = [{"element": k*mx*my + j*mx + i, "constantVector": [0,0,0], "face": "2-"} for j in range(my) for i in range(mx)]

def update_neumann_bc(t):
  k = 0
  factor = min(1, t/variables.force_time_span)   # for t ∈ [0,100] from 0 to force in 100 ms
  elasticity_neumann_bc = [{
		"element": k*mx*my + j*mx + i, 
		"constantVector": [0,0,-variables.force*factor], 		
		"face": "2-",
    "isInReferenceConfiguration": True
  } for j in range(my) for i in range(mx)]

  config = {
    "inputMeshIsGlobal": True,
    "divideNeumannBoundaryConditionValuesByTotalArea": variables.divideNeumannBoundaryConditionValuesByTotalArea,            # if the given Neumann boundary condition values under "neumannBoundaryConditions" are total forces instead of surface loads and therefore should be scaled by the surface area of all elements where Neumann BC are applied
    "neumannBoundaryConditions": elasticity_neumann_bc,
  }
  return config


config = {
  "scenarioName":                   variables.scenario_name,      # scenario name to identify the simulation runs in the log file
  "logFormat":                      "csv",                        # "csv" or "json", format of the lines in the log file, csv gives smaller files
  "solverStructureDiagramFile":     "solver_structure.txt",       # output file of a diagram that shows data connection between solvers
  "mappingsBetweenMeshesLogFile":   "mappings_between_meshes_log.txt",    # log file for mappings 
  "Meshes":                         meshes_tendon,
  
  "PreciceAdapter": {        
      "timeStepOutputInterval":   1,                        # interval in which to display current timestep and time in console
      "timestepWidth":            variables.dt_elasticity,                          # coupling time step width, must match the value in the precice config
      "couplingEnabled":          True,                       # if the precice coupling is enabled, if not, it simply calls the nested solver, for debugging
      "preciceConfigFilename":    variables.precice_file,    # the preCICE configuration file
      "preciceParticipantName":   "BottomTendonSolver",             # name of the own precice participant, has to match the name given in the precice xml config file
      "preciceMeshes": [                                      # the precice meshes get created as the top or bottom surface of the main geometry mesh of the nested solver
        {
          "preciceMeshName":      "BottomTendonMesh",            # precice name of the 2D coupling mesh
          "face":                 "2+",                       # face of the 3D mesh where the 2D mesh is located, "2-" = bottom, "2+" = top
        }   
      ],
      "preciceData": [  
        {
          "mode":                 "read-displacements-velocities",   # mode is one of "read-displacements-velocities", "read-traction", "write-displacements-velocities", "write-traction"
          "preciceMeshName":             "BottomTendonMesh",                    # name of the precice coupling surface mesh, as given in the precice xml settings file
          "displacementsName":    "Displacement",                     # name of the displacements "data", i.e. field variable, as given in the precice xml settings file
          "velocitiesName":       "Velocity",                     # name of the velocities "data", i.e. field variable, as given in the precice xml settings file

        },
        {
          "mode":          "write-traction",                    # mode is one of "read-displacements-velocities", "read-traction", "write-displacements-velocities", "write-traction"
          "preciceMeshName":      "BottomTendonMesh",                    # name of the precice coupling surface mesh, as given in the precice xml settings 
          "tractionName":         "Traction",                         # name of the traction "data", i.e. field variable, as given in the precice xml settings file
        }
      ],

  "DynamicHyperelasticitySolver": {
      "timeStepWidth":              variables.dt_elasticity,      #variables.dt_elasticity,      # time step width 
      "endTime":                    variables.end_time,           # end time of the simulation time span    
      "durationLogKey":             "duration_mechanics",         # key to find duration of this solver in the log file
      "timeStepOutputInterval":     1,                            # how often the current time step should be printed to console
      
      "materialParameters":         variables.material_parameters,  # material parameters of the Mooney-Rivlin material
      "density":                    variables.rho,  
      "displacementsScalingFactor": 1.0,                          # scaling factor for displacements, only set to sth. other than 1 only to increase visual appearance for very small displacements
      "residualNormLogFilename":    "log_residual_norm.txt",      # log file where residual norm values of the nonlinear solver will be written
      "useAnalyticJacobian":        True,                         # whether to use the analytically computed jacobian matrix in the nonlinear solver (fast)
      "useNumericJacobian":         False,                        # whether to use the numerically computed jacobian matrix in the nonlinear solver (slow), only works with non-nested matrices, if both numeric and analytic are enable, it uses the analytic for the preconditioner and the numeric as normal jacobian
      
      "dumpDenseMatlabVariables":   False,                        # whether to have extra output of matlab vectors, x,r, jacobian matrix (very slow)
      # if useAnalyticJacobian,useNumericJacobian and dumpDenseMatlabVariables all all three true, the analytic and numeric jacobian matrices will get compared to see if there are programming errors for the analytic jacobian
      
      # mesh
      "meshName":                   "tendon_Mesh_quadratic",           # mesh with quadratic Lagrange ansatz functions
      "inputMeshIsGlobal":          True,                         # boundary conditions are specified in global numberings, whereas the mesh is given in local numberings
      
      "fiberMeshNames":             [],                           # fiber meshes that will be used to determine the fiber direction
      "fiberDirection":             [0,0,1],                      # if fiberMeshNames is empty, directly set the constant fiber direction, in element coordinate system
      
      # nonlinear solver
      "relativeTolerance":          variables.linear_relative_tolerance,                         # 1e-10 relative tolerance of the linear solver
      "absoluteTolerance":          variables.linear_absolute_tolerance,                        # 1e-10 absolute tolerance of the residual of the linear solver       
      "solverType":                 variables.elasticity_solver_type,                    # type of the linear solver: cg groppcg pipecg pipecgrr cgne nash stcg gltr richardson chebyshev gmres tcqmr fcg pipefcg bcgs ibcgs fbcgs fbcgsr bcgsl cgs tfqmr cr pipecr lsqr preonly qcg bicg fgmres pipefgmres minres symmlq lgmres lcd gcr pipegcr pgmres dgmres tsirm cgls
      "preconditionerType":         variables.elasticity_preconditioner_type,                         # type of the preconditioner
      "maxIterations":              variables.linear_max_iterations,                          # maximum number of iterations in the linear solver
      "snesMaxFunctionEvaluations": variables.snes_max_function_evaluations,                          # maximum number of function iterations
      "snesMaxIterations":          variables.snes_max_iterations,                           # maximum number of iterations in the nonlinear solver
      "snesRelativeTolerance":      variables.snes_relative_tolerance,                         # relative tolerance of the nonlinear solver
      "snesLineSearchType":         variables.snes_linear_search,                         # type of linesearch, possible values: "bt" "nleqerr" "basic" "l2" "cp" "ncglinear"
      "snesAbsoluteTolerance":      variables.snes_absolute_tolerance,                         # absolute tolerance of the nonlinear solver
      "snesRebuildJacobianFrequency":   variables.snes_rebuild_jacobian_frequency,      "relativeTolerance":          variables.linear_relative_tolerance,                         # 1e-10 relative tolerance of the linear solver
      
      #"dumpFilename": "out/r{}/m".format(sys.argv[-1]),          # dump system matrix and right hand side after every solve
      "dumpFilename":               "",                           # dump disabled
      "dumpFormat":                 "matlab",                     # default, ascii, matlab
      
      #"loadFactors":                [0.1, 0.2, 0.35, 0.5, 1.0],   # load factors for every timestep
      #"loadFactors":                [0.5, 1.0],                   # load factors for every timestep
      "loadFactors":                [],                           # no load factors, solve problem directly
      "loadFactorGiveUpThreshold":  1e-3, 
      "nNonlinearSolveCalls":       1,                            # how often the nonlinear solve should be called
      
      # boundary and initial conditions
      "dirichletBoundaryConditions": variables.elasticity_dirichlet_bc,   # the initial Dirichlet boundary conditions that define values for displacements u and velocity v
      "neumannBoundaryConditions": variables.elasticity_neumann_bc,     # Neumann boundary conditions that define traction forces on surfaces of elements
      "divideNeumannBoundaryConditionValuesByTotalArea": False,    # if the initial values for the dynamic nonlinear problem should be computed by extrapolating the previous displacements and velocities
      "updateDirichletBoundaryConditionsFunction": None, #update_dirichlet_bc,   # function that updates the dirichlet BCs while the simulation is running
      "updateDirichletBoundaryConditionsFunctionCallInterval": 1,         # stide every which step the update function should be called, 1 means every time step
      "updateNeumannBoundaryConditionsFunction": update_neumann_bc,       # a callback function to periodically update the Neumann boundary conditions
      "updateNeumannBoundaryConditionsFunctionCallInterval": 1,           # every which step the update function should be called, 1 means every time step 
      
      "constantBodyForce":           None,       # a constant force that acts on the whole body, e.g. for gravity
      "initialValuesDisplacements":  [[0.0,0.0,0.0] for _ in range(nx * ny * nz)],     # the initial values for the displacements, vector of values for every node [[node1-x,y,z], [node2-x,y,z], ...]
      "initialValuesVelocities":     [[0.0,0.0,0.0] for _ in range(nx * ny * nz)],     # the initial values for the velocities, vector of values for every node [[node1-x,y,z], [node2-x,y,z], ...]
      "extrapolateInitialGuess":     True, 

      "dirichletOutputFilename":     "out/" + variables.scenario_name + "_dirichlet_boundary_conditions_tendon",    # filename for a vtp file that contains the Dirichlet boundary condition nodes and their values, set to None to disable

      "OutputWriter" : [
          {"format": "Paraview", "outputInterval": variables.output_timestep_elasticity, "filename": "out/" + variables.scenario_name + "_mechanics_3D", "binary": True, "fixedFormat": False, "onlyNodalValues":True, "combineFiles":True, "fileNumbering": "incremental"},
      ],
      "pressure": {   # output files for pressure function space (linear elements), contains pressure values, as well as displacements and velocities
          "OutputWriter" : [
              # {"format": "Paraview", "outputInterval": 1, "filename": "out/"+variables.scenario_name+"/p", "binary": True, "fixedFormat": False, "onlyNodalValues":True, "combineFiles":True, "fileNumbering": "incremental"},
          ]
      },
      "dynamic": {    # output of the dynamic solver, has additional virtual work values 
          "OutputWriter" : [   # output files for displacements function space (quadratic elements)
          
          ],
      },
      "LoadIncrements": {   
          "OutputWriter" : [
              #{"format": "Paraview", "outputInterval": 1, "filename": "out/load_increments", "binary": False, "fixedFormat": False, "onlyNodalValues":True, "combineFiles":True, "fileNumbering": "incremental"},
          ]
      },
    }
  }
}
