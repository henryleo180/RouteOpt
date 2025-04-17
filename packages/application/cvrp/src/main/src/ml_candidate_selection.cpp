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
    std::vector<std::pair<int, int>>  CVRPSolver::getBestTwoEdges(
        BbNode *node,
        Branching::BranchingHistory<std::vector<std::pair<int, int>>, VectorPairHasher> &history,
        Branching::BranchingDataShared<std::vector<std::pair<int, int>>, VectorPairHasher> &data_shared,
        Branching::CandidateSelector::BranchingTesting<BbNode, std::vector<std::pair<int, int>>, VectorPairHasher> &tester
    ) {
        // 1) Build the 2‑edge map: each key is a vector of two edges, value is their sum.
        auto comboMap = BbNode::obtainSol3DEdgeMap(node);

        // 2) If there are no combinations, return empty.
        if (comboMap.empty()) 
            return {};

        // 3) Find the key (two‑edge vector) with sum closest to target.
        const double target = 1.5;
        double bestDiff = std::numeric_limits<double>::infinity();
        std::vector<std::pair<int,int>> bestCombo;

        for (auto &kv : comboMap) {
            double sum = kv.second;
            double diff = std::abs(sum - target);
            if (diff < bestDiff) {
                bestDiff = diff;
                bestCombo = kv.first;  // a vector of exactly two edges
            }
        }

        // 4) Return that two‑edge vector.
        return bestCombo;
    }

}
