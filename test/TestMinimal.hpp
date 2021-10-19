#ifndef TESTMINIMAL_HPP_
#define TESTMINIMAL_HPP_

#define PROBLEM_SPACE_DIM 2
#define PROBLEM_ELEMENT_DIM 2
/**
 * @file
 * This test runs a minimal simulation with Simplified Imtiaz cells in a 2D mesh
 */

#include <cxxtest/TestSuite.h>
/* Most Chaste code uses PETSc to solve linear algebra problems.  This involves starting PETSc at the beginning of a test-suite
 * and closing it at the end.  (If you never run code in parallel then it is safe to replace PetscSetupAndFinalize.hpp with FakePetscSetup.hpp)
 */

#include "Debug.hpp"
#include "NeuralComponents.hpp"
#include "ChasteEllipsoid.hpp"
#include "ChastePoint.hpp"
#include "../src/DummyDerivedCa.hpp"
#include "../src/Du2013_neural.hpp"
#include "AbstractCardiacCellFactory.hpp"
#include "../src/BidomainProblemNeural.hpp"
#include "DistributedTetrahedralMesh.hpp"
#include "TrianglesMeshReader.hpp"

#include "PetscSetupAndFinalize.hpp"

class ICCFactory : public AbstractCardiacCellFactory<PROBLEM_SPACE_DIM>
{
  private:
  std::set<unsigned> setICCNode;

  public:
  ICCFactory(std::set<unsigned> iccNodes) : AbstractCardiacCellFactory<PROBLEM_SPACE_DIM>(), setICCNode(iccNodes)
  {
    TRACE("Number of nodes in cell factory: " << setICCNode.size());
  };

  AbstractCardiacCell* CreateCardiacCellForTissueNode(Node<PROBLEM_SPACE_DIM>* pNode)
  {
    unsigned index = pNode->GetIndex();

    ChastePoint<PROBLEM_SPACE_DIM> centre(0.00,0.025);
    ChastePoint<PROBLEM_SPACE_DIM> radii (0.01,0.01);
    ChasteEllipsoid<PROBLEM_SPACE_DIM> pacemaker(centre, radii);
    
    if(setICCNode.find(index) != setICCNode.end())
    {
      CellDu2013_neuralFromCellML* cell = new CellDu2013_neuralFromCellML(mpSolver, mpZeroStimulus);
      
      if (pacemaker.DoesContain(pNode->GetPoint()))
      {
        cell->SetParameter("cor", 1.2);
      }

      return cell;

    }

    return new DummyDerivedCa(mpSolver, mpZeroStimulus);

  };
};

class TestHello : public CxxTest::TestSuite
{
  public:
  void TestMinimalSimulation()
  {

    // -------------- OPTIONS ----------------- //
    std::string mesh_ident = "MeshNetwork-2D-85Nodes-144Elems";
    std::string output_dir = mesh_ident + "-2DSerial";
    double duration = 50.0;      // ms
    double print_step = 5.0;        // ms
    // ---------------------------------------- //

    // Mesh location
    std::string mesh_dir = "projects/mesh/ICC2D/" + mesh_ident;
    TrianglesMeshReader<2,2> mesh_reader(mesh_dir.c_str());

    // Initialise mesh variables
    std::set<unsigned> iccNodes;
    unsigned nElements = 0;
    DistributedTetrahedralMesh<PROBLEM_ELEMENT_DIM,PROBLEM_SPACE_DIM> mesh;

    // Cell labels
    std::set<unsigned> ICC_id;
    ICC_id.insert(1);
    std::set<unsigned> bath_id;
    bath_id.insert(0);

    // Construct ICC mesh network from mesh file
    mesh.ConstructFromMeshReader(mesh_reader);
    nElements = mesh.GetNumLocalElements();

    // Define boundary nodes as bath
    double eleIdentify = 0;
    for (DistributedTetrahedralMesh<2,2>::ElementIterator iter = mesh.GetElementIteratorBegin(); iter != mesh.GetElementIteratorEnd(); ++iter)
    {
      eleIdentify = iter->GetAttribute();
      if (eleIdentify == 1) // ICC=1 and Bath=0
      {
        if(!iter->GetNode(0)->IsBoundaryNode())
        {
          iccNodes.insert(iter->GetNodeGlobalIndex(0));
        }
        if(!iter->GetNode(1)->IsBoundaryNode())
        {
          iccNodes.insert(iter->GetNodeGlobalIndex(1));
        }
        // 2D has two nodes per element for line elements?
        if(!iter->GetNode(2)->IsBoundaryNode())
        {
          iccNodes.insert(iter->GetNodeGlobalIndex(2));
        }
      }
    }

    // Print mesh summary
    TRACE("Number of elements: " << nElements);
    TRACE("Number of ICC nodes: " << iccNodes.size());
    TRACE("Total number of nodes: " << mesh.GetNumAllNodes());

    // TODO: Load neural info and set up ParamConfig singleton instance
    // NeuralData eData = NeuralData("projects/NeuralData/T1_e.txt", 1, 2, 3, 0.12, 0.36); // this will be a pre-processed (in Python) table instead of a histogram
    // maybe possible to call Python pre-processor on the fly at runtime (future work)
    // ParamConfig::SetExcitatory("path/to/preprocessed_e.txt");
    // ParamConfig::SetInhibitory("path/to/preprocessed_i.txt");
    // ParamConfig::SetupInfluenceRegionGrid(width, length, width_bins, length_bins)
    // ParamConfig::SetInputTimestep(stepInMillisec)



    // Initialise problem with cells
    ICCFactory network_cells(iccNodes);
    BidomainProblemNeural<PROBLEM_SPACE_DIM> bidomain_problem(&network_cells, true);
    bidomain_problem.SetMesh( &mesh );

    // Modify simulation config
    HeartConfig::Instance()->Reset();
    HeartConfig::Instance()->SetSimulationDuration(duration);
    HeartConfig::Instance()->SetOutputDirectory(output_dir.c_str());
    HeartConfig::Instance()->SetOutputFilenamePrefix("results");
    HeartConfig::Instance()->SetTissueAndBathIdentifiers(ICC_id, bath_id);
    HeartConfig::Instance()->SetIntracellularConductivities(Create_c_vector(0.12, 0.12)); // these are quite smaller than cm values
    HeartConfig::Instance()->SetExtracellularConductivities(Create_c_vector(0.2, 0.2)); // these are quite smaller than cm values
    HeartConfig::Instance()->SetSurfaceAreaToVolumeRatio(2000);
    HeartConfig::Instance()->SetCapacitance(2.5);
    HeartConfig::Instance()->SetVisualizeWithMeshalyzer(true);
    HeartConfig::Instance()->SetOdePdeAndPrintingTimeSteps(0.1, 0.1, print_step);

    // Update problem from config
    bidomain_problem.SetWriteInfo();
    bidomain_problem.Initialise();    // resets initial conditions and time to 0.0 ms

    TRACE("Starting Solve");
    // Solve problem
    bidomain_problem.Solve();

    // Print summary to terminal
    HeartEventHandler::Headings();
    HeartEventHandler::Report();
  };

};

#endif /*TESTMINIMAL_HPP_*/