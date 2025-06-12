/*
 * Copyright (c) 2025 Zhengzhong (Ricky) You.
 * All rights reserved.
 * Software: RouteOpt
 * License: GPL-3.0
 */

#include <numeric>
#include "cvrp.hpp"

namespace RouteOpt::Application::CVRP
{
    namespace ImposeBranchingDetail
    {
        inline void deleteArcByFalseBranchConstraint(int num_buckets_per_vertex, Bucket **buckets,
                                                     const std::pair<int, int> &edge)
        {
            int i = edge.first, j = edge.second;
            bool if_rep = false;
        AGAIN:
            for (int bin = 0; bin < num_buckets_per_vertex; ++bin)
            {
                auto if_find = std::find(buckets[i][bin].bucket_arcs.begin(),
                                         buckets[i][bin].bucket_arcs.end(), j);
                if (if_find == buckets[i][bin].bucket_arcs.end())
                {
                    auto iff = std::find_if(buckets[i][bin].jump_arcs.begin(),
                                            buckets[i][bin].jump_arcs.end(),
                                            [&](const std::pair<res_int, int> &p)
                                            { return p.second == j; });
                    if (iff != buckets[i][bin].jump_arcs.end())
                    {
                        buckets[i][bin].jump_arcs.erase(iff);
                    }
                }
                else
                {
                    buckets[i][bin].bucket_arcs.erase(if_find);
                }
            }
            if (!if_rep)
            {
                if_rep = true;
                std::swap(i, j);
                goto AGAIN;
            }
        }

        void addBranchCutToUnsolved(
            BbNode *node,
            BbNode *&node2,
            const std::pair<int, int> &info,
            int num_buckets_per_vertex,
            bool if_symmetry)
        {
            if (node->getIfTerminate())
                return;

            int num_row;
            SAFE_SOLVER(node->refSolver().getNumRow(&num_row))
            Brc bf;
            bf.edge = info;
            bf.idx_brc = num_row;
            bf.if_3b_mid = false;
            bf.if_2b = true;
            std::vector<int> solver_ind;
            std::vector<double> solver_val;
            node->obtainBrcCoefficient(info, solver_ind, solver_val);

            bf.br_dir = true;
            node2 = new BbNode(node, if_symmetry, num_buckets_per_vertex, bf);
            TestingDetail::addBranchConstraint(solver_ind, solver_val, node2->refSolver(), true);

            bf.br_dir = false;
            bf.idx_brc = INVALID_BRC_INDEX;
            bf.if_3b_mid = false;
            bf.if_2b = true;
            node->refBrCs().emplace_back(bf);
            if (solver_ind.front() == 0)
            {
                solver_ind.erase(solver_ind.begin());
            }
            node->rmLPCols(solver_ind);

            deleteArcByFalseBranchConstraint(num_buckets_per_vertex, node->refAllForwardBuckets(),
                                             info);
            if (!if_symmetry)
            {
                deleteArcByFalseBranchConstraint(num_buckets_per_vertex,
                                                 node->refAllBackwardBuckets(),
                                                 info);
            }
            node->getNewIdx();
        }

        void addBranchCutToUnsolvedInEnu(BbNode *node, BbNode *&node2, const std::pair<int, int> &info,
                                         int *col_pool4_pricing)
        {
            if (node->getIfTerminate())
                return;

            std::vector<int> ind_use;
            std::vector<double> solver_val;
            node->obtainBrcCoefficient(info, ind_use, solver_val);
            if (ind_use.front() == 0)
            {
                ind_use.erase(ind_use.begin());
            }

            std::vector<int> ind_not_allow;
            node->obtainColIdxNotAllowByEdge(info, ind_not_allow);
            if (ind_not_allow.front() == 0)
            {
                ind_not_allow.erase(ind_not_allow.begin());
            }

            int num_row;
            SAFE_SOLVER(node->refSolver().getNumRow(&num_row))
            std::vector<double> duals(num_row, -1); // to prevent any constraints are deleted!

            Brc bf;
            bf.edge = info;
            bf.br_dir = true;
            bf.idx_brc = INVALID_BRC_INDEX;
            node2 = new BbNode(node, bf);
            node2->rmLPCols(ind_not_allow);
            if (node->refIndexColumnsInEnumerationColumnPool().size() != 0)
            {
                node->rmColByBranchInEnuMatrix(node2->refDeletedColumnsInEnumerationColumnPool(), true, {bf},
                                               col_pool4_pricing);
                BbNode::regenerateEnumMat(node, node2, false, duals);
            }

            bf.br_dir = false;
            node->refBrCs().emplace_back(bf);
            node->rmLPCols(ind_use);
            if (node->refIndexColumnsInEnumerationColumnPool().size() != 0)
            {
                node->rmColByBranchInEnuMatrix(node->refDeletedColumnsInEnumerationColumnPool(), true, {bf},
                                               col_pool4_pricing);
                BbNode::regenerateEnumMat(node, node, false, duals);
            }
            node->getNewIdx();
        }
    }

    void CVRPSolver::imposeBranching(BbNode *node, const std::pair<int, int> &brc, std::vector<BbNode *> &children)
    {
        children.resize(2);
        children.front() = node;

        if (node->getIfInEnumState())
        {
            ImposeBranchingDetail::addBranchCutToUnsolvedInEnu(
                node, children.back(), brc, pricing_controller.getColumnPoolPtr());
        }
        else
        {
            ImposeBranchingDetail::addBranchCutToUnsolved(
                node, children.back(), brc, pricing_controller.getNumBucketPerVertex(), !IF_SYMMETRY_PROHIBIT);
        }
        if (children.back() == nullptr)
            children.pop_back();
        node->clearEdgeMap();
    };

    void CVRPSolver::imposeThreeBranching(
        BbNode *node,
        const std::vector<std::pair<int, int>> &edgepair,
        std::vector<BbNode *> &children,
        const double lp_sum)
    {
        // assert(edgepair.size() == 2);
        assert(edgepair.size() <= 2);
        // auto e1 = edgepair[0];
        // auto e2 = edgepair[1];

        // static constexpr int MAX_3WAY_ROUNDS = 13;
        // static thread_local int three_way_count = 0;

        if (node->getIfTerminate())
            return;

        if (edgepair.size() == 2)
        { // then do the three branching when input size is 2
            children.clear();
            children.reserve(3);
            auto e1 = edgepair[0];
            auto e2 = edgepair[1];
            // 1. child A shoud be node;;;
            //  Branch A: enforce e1 = 1 and e2 = 1
            {

                int row1;
                SAFE_SOLVER(node->refSolver().getNumRow(&row1));
                Brc bf1{e1, row1, true, false, false};

                // 2) Build the child with bf1 exactly as your 2‐way routine
                BbNode *childA = new BbNode(
                    node,
                    /*if_symmetry=*/!IF_SYMMETRY_PROHIBIT,
                    pricing_controller.getNumBucketPerVertex(),
                    bf1);
                // At this point childA->brcs has all of node->brcs plus bf1.

                // 3) Add the LP row for e1
                {
                    std::vector<int> e1_cind;
                    std::vector<double> e1_cval;
                    childA->obtainBrcCoefficient(e1, e1_cind, e1_cval);
                    TestingDetail::addBranchConstraint(e1_cind, e1_cval,
                                                       childA->refSolver(),
                                                       /*true=*/true);
                }
                int row2;
                SAFE_SOLVER(childA->refSolver().getNumRow(&row2));

                Brc bf2{e2, row2, true, false, false};

                // 5) *Manually* push bf2 into childA’s brcs list
                childA->refBrCs().emplace_back(bf2);

                // 6) Add the LP row for e2
                {
                    std::vector<int> e2_cind;
                    std::vector<double> e2_cval;
                    childA->obtainBrcCoefficient(e2, e2_cind, e2_cval);
                    TestingDetail::addBranchConstraint(e2_cind, e2_cval,
                                                       childA->refSolver(),
                                                       /*true=*/true);
                }

                children.push_back(childA);
                PRINT_REMIND("Branch A: " + std::to_string(e1.first) + " " + std::to_string(e1.second) + " " +
                             std::to_string(e2.first) + " " + std::to_string(e2.second));
            }

            // Branch B: enforce e1 = 0 and e2 = 0
            {
                // 1) Reserve row for the first false‐branch on e1
                int row1;
                SAFE_SOLVER(node->refSolver().getNumRow(&row1));
                Brc bf1{e1, row1, /*br_dir=*/false, false, false};

                // 2) Build the child with bf1, exactly like addBranchCutToUnsolved
                BbNode *childB = new BbNode(
                    node,
                    /*if_symmetry=*/!IF_SYMMETRY_PROHIBIT,
                    pricing_controller.getNumBucketPerVertex(),
                    bf1);
                // childB->brcs now = parent.brscs + bf1

                // 3) Add the LP row enforcing x_{e1} = 0
                {
                    std::vector<int> inds1;
                    std::vector<double> vals1;
                    childB->obtainBrcCoefficient(e1, inds1, vals1);
                    TestingDetail::addBranchConstraint(
                        inds1,
                        vals1,
                        childB->refSolver(),
                        /*branchIsTrue=*/false);
                }

                // 4) Reserve row for the second false‐branch on e2
                int row2;
                SAFE_SOLVER(childB->refSolver().getNumRow(&row2));
                Brc bf2{e2, row2, /*br_dir=*/false, false, false};

                // 5) Record bf2 in the child’s branching history
                childB->refBrCs().emplace_back(bf2);

                // 6) Add the LP row enforcing x_{e2} = 0
                {
                    std::vector<int> inds2;
                    std::vector<double> vals2;
                    childB->obtainBrcCoefficient(e2, inds2, vals2);
                    TestingDetail::addBranchConstraint(
                        inds2,
                        vals2,
                        childB->refSolver(),
                        /*branchIsTrue=*/false);
                }

                // 7) Push the both‐false child into your children list
                children.push_back(childB);
                PRINT_REMIND("Branch B: " + std::to_string(e1.first) + " " + std::to_string(e1.second) + " " +
                             std::to_string(e2.first) + " " + std::to_string(e2.second));
            }

            // Branch C: enforce x_e1 + x_e2 = 1
            {

                // PRINT_REMIND("Branch C: three way count = " + std::to_string(three_way_count));

                // 1) Create the child with the first Brc (e1)
                // ------------------------------------------------
                // Reserve the next row index in the solver
                int row;
                SAFE_SOLVER(node->refSolver().getNumRow(&row));

                // Build and push the first branch‐cut (edge=e1, true‐branch)
                Brc bf1{e1, row, /*br_dir=*/true, true, false};
                BbNode *childC = new BbNode(
                    node,
                    /*if_symmetry=*/!IF_SYMMETRY_PROHIBIT,
                    pricing_controller.getNumBucketPerVertex(),
                    bf1);
                // childC->brcs now == parent.brscs + bf1

                // 2) Record the second Brc (e2) against *the same* row
                // ----------------------------------------------------
                Brc bf2{e2, row, /*br_dir=*/true, true, false};
                childC->refBrCs().emplace_back(bf2);
                // childC->brcs now has two new entries, both pointing to row

                // 3) Merge the two sets of coefficients
                // -------------------------------------
                std::vector<int> inds1, inds2;
                std::vector<double> vals1, vals2;
                childC->obtainBrcCoefficient(e1, inds1, vals1);
                childC->obtainBrcCoefficient(e2, inds2, vals2);
                int num_col;
                SAFE_SOLVER(node->refSolver().getNumCol(&num_col))

                std::vector<double> tmp_val(num_col, 0);
                for (int i = 0; i < inds1.size(); ++i)
                {
                    tmp_val[inds1[i]] += vals1[i];
                }
                for (int i = 0; i < inds2.size(); ++i)
                {
                    tmp_val[inds2[i]] += vals2[i];
                }
                std::vector<int> cind;
                std::vector<double> cval;

                cind.emplace_back(0);
                cval.emplace_back(1.0);
                for (int i = 1; i < num_col; ++i)
                {
                    if (tmp_val[i] > 0)
                    {
                        cind.emplace_back(i);
                        cval.emplace_back(tmp_val[i]);
                    }
                }

                SAFE_SOLVER(childC->refSolver().addConstraint(static_cast<int>(cind.size()),
                                                              cind.data(),
                                                              cval.data(),
                                                              SOLVER_EQUAL,
                                                              1.0,
                                                              nullptr));

                SAFE_SOLVER(childC->refSolver().updateModel());

                // 5) That single child covers the “exactly one” case
                children.push_back(childC);

                PRINT_REMIND("Third branch added");
            }

            for (auto &node : children)
            {
                node->clearEdgeMap();
            }
        }

        else if (edgepair.size() == 1)
        {
            auto brc = edgepair[0];
            // --- New Branch A: do a 2‑way split on (e1,e2) → 2 children ---
            // imposeBranching(node, e1, children);
            children.resize(2);
            children.front() = node;

            if (node->getIfInEnumState())
            {
                ImposeBranchingDetail::addBranchCutToUnsolvedInEnu(
                    node, children.back(), brc, pricing_controller.getColumnPoolPtr());
            }
            else
            {
                ImposeBranchingDetail::addBranchCutToUnsolved(
                    node, children.back(), brc, pricing_controller.getNumBucketPerVertex(), !IF_SYMMETRY_PROHIBIT);
            }
            if (children.back() == nullptr)
                children.pop_back();
            node->clearEdgeMap();
        }
        else
        {
            // --- New Branch C’: do a 2‑way split on (e1,e2) → 2 children ---

            PRINT_REMIND("No edge pair provided, skipping branching");
        }
    }

    BbNode::BbNode(BbNode *node,
                   bool if_symmetry,
                   int num_buckets_per_vertex,
                   const Brc &bf)
    {
        node->if_root_node = false; // start branching
        idx = ++node_idx_counter;
        cols = node->cols;
        solver.getSolver(&node->solver);
        rccs = node->rccs;
        r1cs = node->r1cs;
        brcs = node->brcs;
        brcs.emplace_back(bf);
        value = node->value;
        num_forward_bucket_arcs = node->num_forward_bucket_arcs;
        num_forward_jump_arcs = node->num_forward_jump_arcs;
        topological_order_forward = node->topological_order_forward;
        if (!if_symmetry)
        {
            topological_order_backward = node->topological_order_backward;
            num_backward_bucket_arcs = node->num_backward_bucket_arcs;
            num_backward_jump_arcs = node->num_backward_jump_arcs;
        }
        last_gap = node->last_gap;

        all_forward_buckets = new Bucket *[dim];
        if (node->all_forward_buckets == nullptr)
            std::terminate();
        for (int i = 0; i < dim; ++i)
        {
            all_forward_buckets[i] = new Bucket[num_buckets_per_vertex];
            for (int j = 0; j < num_buckets_per_vertex; ++j)
            {
                all_forward_buckets[i][j] = node->all_forward_buckets[i][j];
            }
        }
        if (!if_symmetry)
        {
            if (node->all_backward_buckets == nullptr)
                std::terminate();
            all_backward_buckets = new Bucket *[dim];
            for (int i = 0; i < dim; ++i)
            {
                all_backward_buckets[i] = new Bucket[num_buckets_per_vertex];
                for (int j = 0; j < num_buckets_per_vertex; ++j)
                {
                    all_backward_buckets[i][j] = node->all_backward_buckets[i][j];
                }
            }
        }
    }

    BbNode::BbNode(BbNode *node, const Brc &bf)
    {
        node->if_root_node = false; // start branching
        if_in_enu_state = true;
        idx = ++node_idx_counter;
        rccs = node->rccs;
        r1cs = node->r1cs;
        brcs = node->brcs;
        last_gap = node->last_gap;
        brcs.emplace_back(bf);

        solver.getSolver(&node->refSolver());
        valid_size = node->valid_size;

        index_columns_in_enumeration_column_pool = node->index_columns_in_enumeration_column_pool;
        cost_for_columns_in_enumeration_column_pool = node->cost_for_columns_in_enumeration_column_pool;
        deleted_columns_in_enumeration_pool.assign(node->deleted_columns_in_enumeration_pool.begin(),
                                                   node->deleted_columns_in_enumeration_pool.begin() + node->index_columns_in_enumeration_column_pool.size());
        cols = node->cols;
        value = node->value;
    }
}
