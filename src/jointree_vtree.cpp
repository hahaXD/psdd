#include "psdd/jointree_vtree.h"

#include <assert.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

#include "htd/main.hpp"

namespace {
class FitnessFunction : public htd::ITreeDecompositionFitnessFunction {
 public:
  FitnessFunction(void) {}

  ~FitnessFunction() {}

  htd::FitnessEvaluation* fitness(
      const htd::IMultiHypergraph& graph,
      const htd::ITreeDecomposition& decomposition) const {
    HTD_UNUSED(graph)

    /**
     * Here we specify the fitness evaluation for a given decomposition.
     * In this case, we select the maximum bag size and the height.
     */
    return new htd::FitnessEvaluation(2,
                                      -(double)(decomposition.maximumBagSize()),
                                      -(double)(decomposition.height()));
  }

  FitnessFunction* clone(void) const { return new FitnessFunction(); }
};

Vtree* GetMinFillVtree(UaiNetwork* network, int seed = -1) {
  if (seed == -1) {
    seed = (int)time(nullptr);
  }
  std::srand(seed);
  std::unique_ptr<htd::LibraryInstance> manager(
      htd::createManagementInstance(htd::Id::FIRST));

  const size_t var_size = network->var_size();
  htd::IMutableMultiHypergraph* htd_graph =
      manager->multiHypergraphFactory().createInstance();

  for (auto i = 0; i < var_size; ++i) {
    htd_graph->addVertex();
  }
  const auto& factors = network->factor_scopes();
  for (const auto& f : factors) {
    std::vector<htd::vertex_t> h_e;
    for (auto v : f) {
      h_e.push_back(v);
    }
    htd_graph->addEdge(h_e);
  }
  // Create an instance of the fitness function.
  FitnessFunction fitnessFunction;

  /**
   *  This operation changes the root of a given decomposition so that the
   * provided fitness function is maximized. When no fitness function is
   * provided to the constructor, the constructed optimization operation does
   * not perform any optimization and only applies provided manipulations.
   */
  htd::TreeDecompositionOptimizationOperation* operation =
      new htd::TreeDecompositionOptimizationOperation(manager.get(),
                                                      fitnessFunction.clone());

  /**
   *  Set the previously created management instance to support graceful
   * termination.
   */
  operation->setManagementInstance(manager.get());

  /**
   *  Set the vertex selections strategy (default = exhaustive).
   *
   *  In this case, we want to select (at most) 10 vertices of the input
   * decomposition randomly.
   */
  operation->setVertexSelectionStrategy(
      new htd::RandomVertexSelectionStrategy(20));

  /**
   *  Set desired manipulations. In this case we want a nice (= normalized)
   * tree decomposition.
   */
  operation->addManipulationOperation(
      new htd::NormalizationOperation(manager.get()));

  /**
   *  Optionally, we can set the vertex elimination algorithm.
   *  We decide to use the min-degree heuristic in this case.
   */
  manager->orderingAlgorithmFactory().setConstructionTemplate(
      new htd::MinFillOrderingAlgorithm(manager.get()));

  // Get the default tree decomposition algorithm. One can also choose a
  // custom one.
  htd::ITreeDecompositionAlgorithm* baseAlgorithm =
      manager->treeDecompositionAlgorithmFactory().createInstance();

  /**
   *  Set the optimization operation as manipulation operation in order
   *  to choose the optimal root reducing height of the tree decomposition.
   */
  baseAlgorithm->addManipulationOperation(operation);

  /**
   *  Create a new instance of
   * htd::IterativeImprovementTreeDecompositionAlgorithm based on the base
   * algorithm and the fitness function. Note that the fitness function can be
   * an arbiraty one and can differ from the one used in the optimization
   * operation.
   */
  htd::IterativeImprovementTreeDecompositionAlgorithm algorithm(
      manager.get(), baseAlgorithm, fitnessFunction.clone());

  /**
   *  Set the maximum number of iterations after which the best decomposition
   * with respect to the fitness function shall be returned. Use value 1 to
   * make the iterative algorithm return the first decomposition found.
   */
  algorithm.setIterationCount(128);

  /**
   *  Set the maximum number of iterations without improvement after which the
   * algorithm returns best decomposition with respect to the fitness function
   * found so far. A limit of 0 aborts the algorithm after the first
   * non-improving solution has been found, i.e. the algorithm will perform a
   * simple hill-climbing approach.
   */
  algorithm.setNonImprovementLimit(16);

  // Record the optimal maximal bag size of the tree decomposition to allow
  // printing the progress.
  std::size_t optimalBagSize = (std::size_t)-1;

  /**
   *  Compute the decomposition. Note that the additional, optional parameter
   * of the function computeDecomposition() in case of
   * htd::IterativeImprovementTreeDecompositionAlgorithm can be used to
   * intercept every new decomposition. In this case we output some
   * intermediate information upon perceiving an improved decompostion.
   */
  htd::ITreeDecomposition* decomposition = algorithm.computeDecomposition(
      *htd_graph, [&](const htd::IMultiHypergraph& graph,
                      const htd::ITreeDecomposition& decomposition,
                      const htd::FitnessEvaluation& fitness) {
        // Disable warnings concerning unused variables.
        HTD_UNUSED(graph)
        HTD_UNUSED(decomposition)

        std::size_t bagSize = -fitness.at(0);

        /**
         *  After each improvement we print the current optimal
         *  width + 1 and the time when the decomposition was found.
         */
        if (bagSize < optimalBagSize) {
          optimalBagSize = bagSize;

          std::chrono::milliseconds::rep msSinceEpoch =
              std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::system_clock::now().time_since_epoch())
                  .count();

          std::cout << "c status " << optimalBagSize << " " << msSinceEpoch
                    << std::endl;
        }
      });

  htd::PostOrderTreeTraversal traversal;
  size_t vtree_idx = 0;
  std::unordered_map<htd::vertex_t, std::vector<Vtree*>> cache;
  traversal.traverse(
      *decomposition, [&](htd::vertex_t vertex, htd::vertex_t parent,
                          std::size_t distanceToRoot) {
        std::set<htd::vertex_t> var_to_rm;
        if (parent != 0) {
          auto parent_bag = decomposition->bagContent(parent);
          for (htd::index_t index = 0; index < parent_bag.size(); ++index) {
            var_to_rm.insert(decomposition->bagContent(parent).at(index));
          }
        }
        std::vector<htd::vertex_t> var_to_add;
        const auto& cur_bag = decomposition->bagContent(vertex);
        for (htd::index_t index = 0; index < cur_bag.size(); ++index) {
          auto var_idx = cur_bag.at(index);
          if (var_to_rm.find(var_idx) == var_to_rm.end()) {
            var_to_add.push_back(var_idx);
          }
        }
        Vtree* rc = nullptr;
        if (cache.find(vertex) != cache.end()) {
          for (Vtree* v : cache[vertex]) {
            if (rc == nullptr) {
              rc = v;
            } else {
              rc = new_internal_vtree(v, rc);
            }
          }
        }
        if (var_to_add.size() > 0) {
          for (htd::vertex_t var_idx : var_to_add) {
            Vtree* var_vtree = new_leaf_vtree(var_idx);
            if (rc == nullptr) {
              rc = var_vtree;
            } else {
              rc = new_internal_vtree(var_vtree, rc);
            }
          }
        }
        if (rc != nullptr) {
          if (cache.find(parent) != cache.end()) {
            cache[parent].push_back(rc);
          } else {
            cache[parent] = {rc};
          }
        }
      });
  delete decomposition;
  delete htd_graph;
  assert(cache[0].size() == 1);
  set_vtree_properties(cache[0][0]);
  assert(sdd_vtree_parent(cache[0][0]) == nullptr);
  return cache[0][0];
}
}  // namespace

Vtree* JointreeVtree::ConstructVtree() { return GetMinFillVtree(network_); }
