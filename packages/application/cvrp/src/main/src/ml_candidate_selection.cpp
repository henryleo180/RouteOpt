/* 
 * Copyright (c) 2025 Zhengzhong (Ricky) You.
 * All rights reserved.
 * Software: RouteOpt
 * License: GPL-3.0
 */

#include "cvrp.hpp"
#include "cvrp_macro.hpp"

namespace RouteOpt::Application::CVRP {
    std::pair<int, int> CVRPSolver::callMLCandidateSelection(BbNode *node,
                                                             Branching::BranchingHistory<std::pair<int, int>,
                                                                 PairHasher> &history,
                                                             Branching::BranchingDataShared<std::pair<int, int>,
                                                                 PairHasher> &data_shared,
                                                             Branching::CandidateSelector::BranchingTesting<BbNode,
                                                                 std::pair<int, int>, PairHasher> &tester) {
        if constexpr (ml_type == ML_TYPE::ML_NO_USE)
            THROW_RUNTIME_ERROR("machine learning module is not enabled, but called");
        int num_col;
        SAFE_SOLVER(node->refSolver().getNumCol(&num_col))
        std::vector<double> x(num_col);
        SAFE_SOLVER(node->refSolver().getX(0, num_col, x.data()))
        std::vector<int> route_length(num_col);
        for (int i = 0; i < num_col; ++i) { 
            route_length[i] = static_cast<int>(node->getCols()[i].col_seq.size());
        }

        data_shared.refCandidateMap() = BbNode::obtainSolEdgeMap(node);

        switch (ml_type) {
            case ML_TYPE::ML_GET_DATA_1:
                l2b_train_controller.generateModelPhase1(node, history, data_shared, tester, node->getTreeSize(),
                                                         x, route_length);
                break;
            case ML_TYPE::ML_GET_DATA_2:
                l2b_train_controller.generateModelPhase2(node, l2b_predict_controller, history, data_shared, tester,
                                                         node->getTreeSize(), node->calculateOptimalGap(ub), x,
                                                         route_length);
                break;
            case ML_TYPE::ML_USE_MODEL:
                l2b_predict_controller.useMLInGeneralFramework(node, history, data_shared, tester,
                                                               node->getTreeSize(), node->calculateOptimalGap(ub), x,
                                                               route_length);
                break;
            default: ;
        }
        l2b_controller.cleanLastData();
        return data_shared.refBranchPair().front();
    }

    // Returns two candidate edges (std::pair<int,int>) from the given node
    // whose accumulated fractional values sum is as close as possible to 1.5.
    inline std::vector<std::pair<int, int>> CVRPSolver::getBestTwoEdges(BbNode *node,
                                                             Branching::BranchingHistory<std::pair<int, int>,
                                                                 PairHasher> &history,
                                                             Branching::BranchingDataShared<std::pair<int, int>,
                                                                 PairHasher> &data_shared,
                                                             Branching::CandidateSelector::BranchingTesting<BbNode,
                                                                 std::pair<int, int>, PairHasher> &tester) {
        // Call the existing function to get the candidate map.
        // The map type is: std::unordered_map<std::pair<int, int>, double, PairHasher>
        auto candidateMap = BbNode::obtainSolEdgeMap(node);

        // Convert the map into a vector so that we can iterate over candidate pairs.
        std::vector<std::pair<std::pair<int, int>, double>> candidates;
        candidates.reserve(candidateMap.size());
        for (const auto &item : candidateMap) {
            candidates.push_back(item);
        }

        // Ensure we have at least two candidates.
        if (candidates.size() < 2)
            return {};  // Alternatively, you may decide to throw an error or return one candidate.

        const double target = 1.5;
        double bestDiff = std::numeric_limits<double>::max();
        std::pair<int, int> bestEdge1{}, bestEdge2{};

        // Iterate over all unique pairs of candidate edges.
        for (size_t i = 0; i < candidates.size(); ++i) {
            for (size_t j = i + 1; j < candidates.size(); ++j) {
                double sum = candidates[i].second + candidates[j].second;
                double diff = std::abs(sum - target);
                if (diff < bestDiff) {
                    bestDiff = diff;
                    bestEdge1 = candidates[i].first;
                    bestEdge2 = candidates[j].first;
                }
            }
        }

        // Return a vector containing the two selected edges.
        return { bestEdge1, bestEdge2 };
    }

}
