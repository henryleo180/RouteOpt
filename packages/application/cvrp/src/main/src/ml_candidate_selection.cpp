/*
 * Copyright (c) 2025 Zhengzhong (Ricky) You.
 * All rights reserved.
 * Software: RouteOpt
 * License: GPL-3.0
 */

#include "cvrp.hpp"
#include "cvrp_macro.hpp"

namespace RouteOpt::Application::CVRP
{
    std::pair<int, int> CVRPSolver::callMLCandidateSelection(BbNode *node,
                                                             Branching::BranchingHistory<std::pair<int, int>,
                                                                                         PairHasher> &history,
                                                             Branching::BranchingDataShared<std::pair<int, int>,
                                                                                            PairHasher> &data_shared,
                                                             Branching::CandidateSelector::BranchingTesting<BbNode,
                                                                                                            std::pair<int, int>, PairHasher> &tester)
    {
        if constexpr (ml_type == ML_TYPE::ML_NO_USE)
            THROW_RUNTIME_ERROR("machine learning module is not enabled, but called");
        int num_col;
        SAFE_SOLVER(node->refSolver().getNumCol(&num_col))
        std::vector<double> x(num_col);
        SAFE_SOLVER(node->refSolver().getX(0, num_col, x.data()))
        std::vector<int> route_length(num_col);
        for (int i = 0; i < num_col; ++i)
        {
            route_length[i] = static_cast<int>(node->getCols()[i].col_seq.size());
        }

        data_shared.refCandidateMap() = BbNode::obtainSolEdgeMap(node);

        switch (ml_type)
        {
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
        default:;
        }
        l2b_controller.cleanLastData();
        return data_shared.refBranchPair().front();
    }

    // Returns two candidate edges (std::pair<int,int>) from the given node
    // whose accumulated fractional values sum is as close as possible to 1.5.
    std::vector<std::pair<int, int>> CVRPSolver::getBestTwoEdges(
        BbNode *node,
        Branching::BranchingHistory<std::vector<std::pair<int, int>>, VectorPairHasher> &history,
        Branching::BranchingDataShared<std::vector<std::pair<int, int>>, VectorPairHasher> &data_shared,
        Branching::CandidateSelector::BranchingTesting<BbNode, std::vector<std::pair<int, int>>, VectorPairHasher> &tester)
    {
        // // 1) Build the 2‑edge map: each key is a vector of two edges, value is their sum.
        // auto comboMap = BbNode::obtainSol3DEdgeMap(node);

        // // 2) If there are no combinations, return empty.
        // if (comboMap.empty())
        //     return {};

        // // 3) Find the key (two‑edge vector) with sum closest to target.
        // const double target = 1.5;
        // double bestDiff = std::numeric_limits<double>::infinity();
        // std::vector<std::pair<int,int>> bestCombo;

        // for (auto &kv : comboMap) {
        //     double sum = kv.second;
        //     double diff = std::abs(sum - target);
        //     if (diff < bestDiff) {
        //         bestDiff = diff;
        //         bestCombo = kv.first;  // a vector of exactly two edges
        //     }
        // }

        // // 4) Return that two‑edge vector.
        // return bestCombo;

        // VERSION 2: closeness
        // 0) Utility to test “strictly fractional” up to tolerance
        // auto isFractional = [&](double x)
        // {
        //     double frac = x - std::floor(x);
        //     return frac > SOL_X_TOLERANCE && frac < 1.0 - SOL_X_TOLERANCE;
        // };

        // // 1) Build the one‐edge map so we can check individual values
        // auto oneEdgeMap = BbNode::obtainSolEdgeMap(node);

        // // 2) Build the two‐edge sum map
        // auto comboMap = BbNode::obtainSol3DEdgeMap(node);
        // if (comboMap.empty())
        //     return {};

        // const double target = 1;
        // double bestDiff = std::numeric_limits<double>::infinity();
        // std::vector<std::pair<int, int>> bestCombo;

        // // 3) Scan, but *only* consider entries where each edge and the sum are fractional
        // for (auto &kv : comboMap)
        // {
        //     const auto &edges = kv.first;     // vector of two pairs
        //     double sum = kv.second;           // their summed value
        //     double v1 = oneEdgeMap[edges[0]]; // individual edge1 value
        //     double v2 = oneEdgeMap[edges[1]]; // individual edge2 value

        //     // skip any pair that isn't properly fractional
        //     if (!isFractional(v1) || !isFractional(v2) || !isFractional(sum))
        //         continue;

        //     double diff = std::abs(sum - target);
        //     if (diff < bestDiff)
        //     {
        //         bestDiff = diff;
        //         bestCombo = edges;
        //     }
        // }

        // return bestCombo;

        // VERSION 3: closeness and fractional

        auto oneEdgeMap = BbNode::obtainSolEdgeMap(node);
        auto comboMap = BbNode::obtainSol3DEdgeMap(node);
        if (comboMap.empty())
            return {};

        const double target = 1.5;
        double bestScore = -1e300;
        std::vector<std::pair<int, int>> bestCombo;

        for (auto &kv : comboMap)
        {
            auto e = kv.first; // {edge1, edge2}
            double v1 = oneEdgeMap[e[0]];
            double v2 = oneEdgeMap[e[1]];
            double sum = kv.second;

            // require both fractional and sum fractional
            if (v1 < SOL_X_TOLERANCE || v1 > 1 - SOL_X_TOLERANCE || v2 < SOL_X_TOLERANCE || v2 > 1 - SOL_X_TOLERANCE || sum < SOL_X_TOLERANCE || sum > 1 - SOL_X_TOLERANCE)
                continue;

            // score components
            double s_sum = -std::abs(sum - target);
            double s_minf = std::min(std::min(v1, 1 - v1), std::min(v2, 1 - v2));
            double s_prod = (v1 * (1 - v1)) * (v2 * (1 - v2));

            // composite: give 70% weight to fractionalness, 30% to closeness
            double score = 0.7 * s_minf + 0.3 * s_sum;
            // you could also try:  score = s_prod - 2.0*std::abs(sum-target);

            if (score > bestScore)
            {
                bestScore = score;
                bestCombo = e;
            }
        }
        return bestCombo;
    }

}
