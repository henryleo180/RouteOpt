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

        /**
         * addBranchCutToUnsolved
         *
         * This function applies a branching decision to the given BbNode `node`.
         * It creates one new child node (pointed to by `node2`) for the "true" branch
         * and modifies the current node itself to represent the "false" branch.
         *
         * @param node Pointer to the current BbNode to be branched.
         * @param node2 Reference to a BbNode pointer, which will be set to the new child node.
         * @param info The branching information (often an edge or variable indices).
         * @param num_buckets_per_vertex Number of arc-buckets per vertex (for forward/backward arcs).
         * @param if_symmetry Indicates whether symmetry is considered (true) or not (false).
         */
        void addBranchCutToUnsolved(
            BbNode *node,
            BbNode *&node2,
            const std::pair<int, int> &info,
            int num_buckets_per_vertex,
            bool if_symmetry)
        {
            // If this node is flagged to terminate (e.g. infeasible or solver stop condition),
            // then do nothing and return immediately.
            if (node->getIfTerminate())
                return;

            // Obtain the current number of rows in the solver, to use for the new branching constraint.
            int num_row;
            SAFE_SOLVER(node->refSolver().getNumRow(&num_row))

            // Prepare the Brc structure to store the branching info.
            Brc bf;
            bf.edge = info;       // The edge/variable on which we're branching
            bf.idx_brc = num_row; // The solver row index where this constraint will be placed

            // Prepare vectors to receive the indices and coefficients for the branch constraint.
            std::vector<int> solver_ind;
            std::vector<double> solver_val;

            // Retrieve the appropriate coefficients for the constraint from the node.
            node->obtainBrcCoefficient(info, solver_ind, solver_val);

            // ------------------------------------------------------
            // TRUE BRANCH: Create a child node that enforces 'bf.br_dir = true'
            // ------------------------------------------------------
            bf.br_dir = true; // Mark this constraint direction as "true"
            node2 = new BbNode(node, if_symmetry, num_buckets_per_vertex, bf);
            // Add the branching constraint to the solver in the child node
            TestingDetail::addBranchConstraint(solver_ind, solver_val, node2->refSolver(), true);

            // ------------------------------------------------------
            // FALSE BRANCH: Modify the current node to represent the complementary condition
            // ------------------------------------------------------
            bf.br_dir = false;              // Mark this constraint direction as "false"
            bf.idx_brc = INVALID_BRC_INDEX; // Indicate no solver row for this branch constraint
            // Add this branching object to the parent node's list of constraints
            node->refBrCs().emplace_back(bf);

            // If the first solver index is 0 (possibly a special case),
            // remove it before removing columns from the LP.
            if (solver_ind.front() == 0)
            {
                solver_ind.erase(solver_ind.begin());
            }

            // Remove these columns from the parent's LP to enforce the "false" branch.
            node->rmLPCols(solver_ind);

            // Delete (or mark infeasible) any forward arcs corresponding to the "false" branch.
            deleteArcByFalseBranchConstraint(num_buckets_per_vertex,
                                             node->refAllForwardBuckets(),
                                             info);

            // If symmetry is disabled, also remove corresponding backward arcs.
            if (!if_symmetry)
            {
                deleteArcByFalseBranchConstraint(num_buckets_per_vertex,
                                                 node->refAllBackwardBuckets(),
                                                 info);
            }

            // Update the node's internal index or ID after applying the "false" branch.
            node->getNewIdx();
        }

        /**
         * addBranchCutToUnsolvedInEnu
         *
         * This function applies a branching decision for a BbNode that is in the "enumeration" state.
         * It creates one new child node (node2) to represent the "true" branch, and modifies the
         * original node to represent the "false" branch. It also handles column removal from both
         * the LP and the enumeration column pool.
         *
         * @param node Pointer to the current BbNode to be branched.
         * @param node2 Reference to a BbNode pointer, which will be set to the new "true branch" node.
         * @param info The branching information (e.g., an edge or variable indices).
         * @param col_pool4_pricing Pointer to a column pool used in the pricing step (for enumeration).
         */
        void addBranchCutToUnsolvedInEnu(BbNode *node, BbNode *&node2, const std::pair<int, int> &info,
                                         int *col_pool4_pricing)
        {
            // If this node is flagged to terminate, do nothing and return immediately.
            if (node->getIfTerminate())
                return;

            // Prepare vectors to hold indices and coefficients for the branching constraint.
            std::vector<int> ind_use;
            std::vector<double> solver_val;
            // Obtain the coefficients for the branch from the node, based on 'info'.
            node->obtainBrcCoefficient(info, ind_use, solver_val);

            // If the first index is 0, remove it. Possibly represents a special offset or dummy column.
            if (ind_use.front() == 0)
            {
                ind_use.erase(ind_use.begin());
            }

            // Obtain the set of column indices that are not allowed for this branch.
            std::vector<int> ind_not_allow;
            node->obtainColIdxNotAllowByEdge(info, ind_not_allow);

            // Similar check for a leading 0 index in 'ind_not_allow'.
            if (ind_not_allow.front() == 0)
            {
                ind_not_allow.erase(ind_not_allow.begin());
            }

            // Get the current number of rows in the solver, used for preserving dual information.
            int num_row;
            SAFE_SOLVER(node->refSolver().getNumRow(&num_row))
            // 'duals' is initialized to -1 for each row, possibly to prevent certain constraints from being deleted.
            std::vector<double> duals(num_row, -1);

            // Define a Brc structure to describe this branch constraint.
            Brc bf;
            bf.edge = info;
            bf.br_dir = true;               // "true" branch direction
            bf.idx_brc = INVALID_BRC_INDEX; // No row index used here for enumeration state

            // ------------------------------------------------------
            // TRUE BRANCH: Create a child node that enforces the 'true' constraint
            // ------------------------------------------------------
            node2 = new BbNode(node, bf);

            // Remove columns in the new child node that are "not allowed" for this "true" branch.
            node2->rmLPCols(ind_not_allow);

            // If the parent node uses an enumeration column pool, remove columns and regenerate the matrix.
            if (node->refIndexColumnsInEnumerationColumnPool().size() != 0)
            {
                node->rmColByBranchInEnuMatrix(node2->refDeletedColumnsInEnumerationColumnPool(),
                                               true,
                                               {bf},
                                               col_pool4_pricing);
                // Rebuild or refresh the child's enumeration matrix with updated columns.
                BbNode::regenerateEnumMat(node, node2, false, duals);
            }

            // ------------------------------------------------------
            // FALSE BRANCH: Modify the current node to represent the complementary condition
            // ------------------------------------------------------
            bf.br_dir = false; // "false" branch direction
            // Record this new branch constraint object in the parent node's constraint list.
            node->refBrCs().emplace_back(bf);

            // Remove columns in the parent node's LP that represent the "true" branch (to enforce "false").
            node->rmLPCols(ind_use);

            // If the parent node uses an enumeration column pool, remove columns and regenerate the matrix.
            if (node->refIndexColumnsInEnumerationColumnPool().size() != 0)
            {
                node->rmColByBranchInEnuMatrix(node->refDeletedColumnsInEnumerationColumnPool(),
                                               true,
                                               {bf},
                                               col_pool4_pricing);
                // Rebuild or refresh the parent's enumeration matrix with updated columns.
                BbNode::regenerateEnumMat(node, node, false, duals);
            }

            // Update the node's internal index (e.g., unique ID) after applying the "false" branch.
            node->getNewIdx();
        }
    }

    /**
     * imposeBranching
     *
     * This function applies a branching step to a given node (`node`), generating up to two child nodes.
     * Depending on whether `node` is in enumeration state, it calls different branching functions.
     * It then stores the new child (if it exists) in `children.back()`.
     *
     * @param node      Pointer to the current branch-and-bound node.
     * @param brc       The branching information (e.g., an edge or a pair<int,int>).
     * @param children  A vector of BbNode pointers where up to two child nodes may be stored.
     */
    void CVRPSolver::imposeBranching(BbNode *node, const std::pair<int, int> &brc, std::vector<BbNode *> &children)
    {
        // Prepare to store 2 children in the vector.
        // The first child is initialized to point to the same node (often a way to handle "false" branch in-place).
        children.resize(2);
        children.front() = node;

        // Decide which branching function to use based on enumeration state.
        // If in enumeration state, call 'addBranchCutToUnsolvedInEnu';
        // otherwise, call the regular 'addBranchCutToUnsolved'.
        if (node->getIfInEnumState())
        {
            ImposeBranchingDetail::addBranchCutToUnsolvedInEnu(
                node,
                children.back(), // This is where the "true" branch is stored
                brc,
                pricing_controller.getColumnPoolPtr());
        }
        else
        {
            ImposeBranchingDetail::addBranchCutToUnsolved(
                node,
                children.back(), // This is where the "true" branch is stored
                brc,
                pricing_controller.getNumBucketPerVertex(),
                !IF_SYMMETRY_PROHIBIT // A flag indicating whether to allow symmetry
            );
        }

        // If the second child (children.back()) was not created (nullptr), remove it from the vector.
        if (children.back() == nullptr)
            children.pop_back();

        // Clear or reset some data related to edges in the original node (likely to finalize the branching step).
        node->clearEdgeMap();

        PRINT_REMIND("impose 2 branching done");
    };

    void CVRPSolver::imposeThreeBranching(
        BbNode *node,
        const std::vector<std::pair<int, int>> &edgepair, // must have size()==2
        std::vector<BbNode *> &children)
    {
        assert(edgepair.size() == 2);
        auto e1 = edgepair[0];
        auto e2 = edgepair[1];

        static constexpr int MAX_3WAY_ROUNDS = 10;
        static thread_local int three_way_count = 0;

        // If node is already terminal, do nothing.
        if (node->getIfTerminate())
            return;

        children.clear();
        children.reserve(3);

        //
        // --- Branch A: e1 = 1 AND e2 = 1 ---
        //
        {
            // // Clone the parent
            // BbNode* childA = new BbNode(*node);

            // // Force x_{e1} = 1
            // {
            //     std::vector<int>   inds;
            //     std::vector<double> vals;
            //     childA->obtainBrcCoefficient(e1, inds, vals);
            //     TestingDetail::addBranchConstraint(inds, vals,
            //                                     childA->refSolver(),
            //                                     /*branchIsTrue=*/true);
            // }
            // // Force x_{e2} = 1
            // {
            //     std::vector<int>   inds;
            //     std::vector<double> vals;
            //     childA->obtainBrcCoefficient(e2, inds, vals);
            //     TestingDetail::addBranchConstraint(inds, vals,
            //                                     childA->refSolver(),
            //                                     /*branchIsTrue=*/true);
            // }

            // children.push_back(childA);

            // We’re going to build childA by applying two true‑branches in sequence.
            // Start from an untouched copy of the parent so we don’t clobber `node` itself.
            // BbNode* working = new BbNode(*node);
            // BbNode* childA  = nullptr;

            // // First branch on e1:
            // ImposeBranchingDetail::addBranchCutToUnsolved(
            //     working,                                // input node
            //     childA,                                 // output child for true‐branch
            //     e1,                                     // edge to branch on
            //     pricing_controller.getNumBucketPerVertex(),
            //     !IF_SYMMETRY_PROHIBIT
            // );
            // // At this point:
            // //  - working has been modified in‐place as the “false” e1=0 branch
            // //  - childA is the “true” e1=1 branch with bf.edge=e1 recorded and constraint added

            // // Now apply the second branch (e2=1) to that child:
            // BbNode* childA2 = nullptr;
            // ImposeBranchingDetail::addBranchCutToUnsolved(
            //     childA,                                // feed in the e1‐true child
            //     childA2,                               // out comes the e2‐true grandchild
            //     e2,
            //     pricing_controller.getNumBucketPerVertex(),
            //     !IF_SYMMETRY_PROHIBIT
            // );
            // // childA now represents e1=1 & e2=0 (false on the second); childA2 is e1=1 & e2=1

            // // Push only the both‐true branch into your children list:
            // children.push_back(childA2);

            // // (Optionally delete the intermediate nodes to avoid leaks)
            // // delete working;  // no longer needed
            // // delete childA;   // that was just the intermediate e1=1,e2=0 node

            // VERSION 3
            // 1) Get next row for the first branch
            int row1;
            SAFE_SOLVER(node->refSolver().getNumRow(&row1));
            Brc bf1{e1, row1, true};

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

            { //     std::string filename2 = "test2.lp";
              // SAFE_SOLVER(
              //     childA->refSolver().write(filename2.c_str()));

                //     TestingDetail::addBranchConstraint(e1_cind, e1_cval,
                //                                        childA->refSolver(),
                //                                        /*true=*/true);

                //                                        std::cout << "childA: e1: " << e1.first << " " << e1.second << ", " << "e2: " << e2.first << " " << e2.second << std::endl;
                // std::vector<int> col_idx = {0, 288, 1202, 1204, 1206, 1214, 1217, 1219, 1220, 1223};
                // std::cout << "ind= " << std::endl;
                // // std::vector<int> e1_cind;
                // // std::vector<double> e1_cval;
                // // childA->obtainBrcCoefficient(e1, e1_cind, e1_cval);
                // for (auto i : e1_cind)
                // {
                //     std::cout << i << " ";
                // }
                // std::cout << std::endl;

                // for (auto i : e1_cind)
                // {
                //     auto &col = node->getCols()[i];
                //     auto &seq = col.col_seq;
                //     for (auto j : seq)
                //     {
                //         std::cout << j << " ";
                //     }
                //     std::cout << std::endl;
                //     if (i > 515)
                //         break;
                // }

                // for (auto i : col_idx)
                // {
                //     auto &col = node->getCols()[i];
                //     auto &seq = col.col_seq;
                //     for (auto j : seq)
                //     {
                //         std::cout << j << " ";
                //     }
                //     std::cout << std::endl;
                // }
                // // write lp file
                // std::string filename = "test.lp";
                // SAFE_SOLVER(
                //     childA->refSolver().write(filename.c_str()));

                // THROW_RUNTIME_ERROR("")}
            }

            // 4) Now prepare and record the second branch‐cut bf2
            int row2;
            SAFE_SOLVER(childA->refSolver().getNumRow(&row2));

            Brc bf2{e2, row2, true};

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

                // {        std::string filename2 = "test2.lp";
                //         SAFE_SOLVER(
                //             childA->refSolver().write(filename2.c_str()));

                //         TestingDetail::addBranchConstraint(e2_cind, e2_cval,
                //                                             childA->refSolver(),
                //                                             /*true=*/true);

                //         std::cout << "childA: e1: " << e1.first << " " << e1.second << ", " << "e2: " << e2.first << " " << e2.second << std::endl;
                //         // std::vector<int> col_idx = {0, 288, 1202, 1204, 1206, 1214, 1217, 1219, 1220, 1223};
                //         // std::cout << "ind= " << std::endl;
                //         // std::vector<int> e2_cind;
                //         // std::vector<double> e2_cval;
                //         // childA->obtainBrcCoefficient(e1, e2_cind, e2_cval);
                //         for (auto i : e2_cind)
                //         {
                //             std::cout << i << " ";
                //         }

                //         std::cout << std::endl;

                //         for (auto i : e2_cind)
                //         {
                //             auto &col = node->getCols()[i];
                //             auto &seq = col.col_seq;
                //             for (auto j : seq)
                //             {
                //                 std::cout << j << " ";
                //             }
                //             std::cout << std::endl;
                //             // if (i > 515)
                //             //     break;
                //         }

                //         // for (auto i : col_idx)
                //         // {
                //         //     auto &col = node->getCols()[i];
                //         //     auto &seq = col.col_seq;
                //         //     for (auto j : seq)
                //         //     {
                //         //         std::cout << j << " ";
                //         //     }
                //         //     std::cout << std::endl;
                //         // }
                //         // write lp file
                //         std::string filename = "test.lp";
                //         SAFE_SOLVER(
                //             childA->refSolver().write(filename.c_str()));

                //         THROW_RUNTIME_ERROR("")}
            }

            // 7) Finally, we have recorded *two* Brcs (for e1 and e2) in childA
            children.push_back(childA);
        }

        //
        // --- Branch B: e1 = 0 AND e2 = 0, mirroring Branch A’s style ---
        {
            // 1) Reserve row for the first false‐branch on e1
            int row1;
            SAFE_SOLVER(node->refSolver().getNumRow(&row1));
            Brc bf1{e1, row1, /*br_dir=*/false};

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
            Brc bf2{e2, row2, /*br_dir=*/false};

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
        }

        //
        // --- Branch C: x_{e1} + x_{e2} = 1 ---
        //
        {
            // BbNode *childC = new BbNode(*node);

            // // 1) Collect coefficients for e1 and e2
            // std::vector<int> inds1, inds2;
            // std::vector<double> vals1, vals2;
            // childC->obtainBrcCoefficient(e1, inds1, vals1);
            // childC->obtainBrcCoefficient(e2, inds2, vals2);

            // // 2) Merge into one constraint: sum_i vals[i] * x[ind[i]] = 1
            // std::vector<int> inds = inds1;
            // std::vector<double> vals = vals1;
            // for (size_t i = 0; i < inds2.size(); ++i)
            // {
            //     auto it = std::find(inds.begin(), inds.end(), inds2[i]);
            //     if (it == inds.end())
            //     {
            //         inds.push_back(inds2[i]);
            //         vals.push_back(vals2[i]);
            //     }
            //     else
            //     {
            //         size_t idx = it - inds.begin();
            //         vals[idx] += vals2[i];
            //     }
            // }

            // // 3) Build addConstraints arguments:
            // int numconstrs = 1;
            // int numnz = (int)inds.size();
            // std::vector<int> cbeg = {0};              // one row starts at offset 0
            // std::vector<char> sense = {SOLVER_EQUAL}; // equality
            // std::vector<double> rhs = {1.0};          // RHS = 1

            // // 4) Call addConstraints + updateModel, checking errors:
            // SAFE_SOLVER(
            //     childC->refSolver().addConstraints(
            //         numconstrs,
            //         numnz,
            //         cbeg.data(),
            //         inds.data(),
            //         vals.data(),
            //         sense.data(),
            //         rhs.data(),
            //         nullptr));
            // SAFE_SOLVER(
            //     childC->refSolver().updateModel());

            // children.push_back(childC);

            // get cols

            //              C288 + C1152 + C1157 + C1158 + C1160 + C1162 + C1163 + C1166 + C1169
            //    + 2 C1172 + C1174 + C1178 + 2 C1179 + C1180 + C1182 + C1185 + C1186
            //    + C1187 + C1188 + 2 C1191 + C1192 + C1195 + C1202 + C1204 + C1206
            //    + C1214 + C1217 + C1219 + C1220 + C1223 + C1236 + C1253 + C1255 + C1257
            //    + C1259 + C1277 + C1294 + C1329 + C1348 + C1374 + C1410 + C1469 + C1477
            //    + C1491 + C1495 + C1608 + C1615 + C1631 + C1647 + C1725 + C1746 + C1778
            //    + C1794 + C1807 + C1814 + C1826 + C1829 + C1842 + C1864 + C1888 + C1898
            //    + C1915 + C1959 = 1
            // print e1 and e2;
            // std::cout << "e1: " << e1.first << " " << e1.second << ", " << "e2: " << e2.first << " " << e2.second << std::endl;
            // std::vector<int> col_idx = {288, 1152, 1157, 1158, 1160, 1162, 1163, 1166, 1169};
            // for (auto i : col_idx)
            // {
            //     auto &col = node->getCols()[i];
            //     auto &seq = col.col_seq;
            //     for (auto i : seq)
            //     {
            //         std::cout << i << " ";
            //     }
            //     std::cout << std::endl;
            // }
            // // write lp file
            // std::string filename = "test.lp";
            // SAFE_SOLVER(
            //     childC->refSolver().write(filename.c_str()));

            // THROW_RUNTIME_ERROR("")

            // VERSION 3
            // --- Branch C: x_{e1} + x_{e2} = 1, record two Brcs but add one row ---

            // increment how many 3‑way nodes we’ve done on this path
            ++three_way_count;

            if (three_way_count <= MAX_3WAY_ROUNDS)
            {
                // 1) Create the child with the first Brc (e1)
                // ------------------------------------------------
                // Reserve the next row index in the solver
                int row;
                SAFE_SOLVER(node->refSolver().getNumRow(&row));

                // Build and push the first branch‐cut (edge=e1, true‐branch)
                Brc bf1{e1, row, /*br_dir=*/true};
                BbNode *childC = new BbNode(
                    node,
                    /*if_symmetry=*/!IF_SYMMETRY_PROHIBIT,
                    pricing_controller.getNumBucketPerVertex(),
                    bf1);
                // childC->brcs now == parent.brscs + bf1

                // 2) Record the second Brc (e2) against *the same* row
                // ----------------------------------------------------
                Brc bf2{e2, row, /*br_dir=*/true};
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

                std::vector<double> tmp_val(num_col,0);
                for(int i=0; i<inds1.size(); ++i)
                {
                    tmp_val[inds1[i]] += vals1[i];
                }
                for(int i=0; i<inds2.size(); ++i)
                {
                    tmp_val[inds2[i]] += vals2[i];
                }
                std::vector<int> cind;
                std::vector<double> cval;

                cind.emplace_back(0);
                cval.emplace_back(1.0);
                for(int i=1; i<num_col; ++i)
                {
                    if(tmp_val[i] > 0)
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

            else
            {
                // --- New Branch C’: do a 2‑way split on (e1,e2) → 2 children ---

                // Child C1: enforce e1=1, e2=0
                {
                    // Reserve row1 & build child for e1=1
                    int row1;
                    SAFE_SOLVER(node->refSolver().getNumRow(&row1));
                    Brc bf1{e1, row1, true};
                    BbNode *childC1 = new BbNode(node,
                                                 !IF_SYMMETRY_PROHIBIT,
                                                 pricing_controller.getNumBucketPerVertex(),
                                                 bf1);
                    // enforce x_{e1}=1
                    {
                        std::vector<int> i1;
                        std::vector<double> v1;
                        childC1->obtainBrcCoefficient(e1, i1, v1);
                        TestingDetail::addBranchConstraint(i1, v1,
                                                           childC1->refSolver(), /*true=*/true);
                    }

                    // record and enforce x_{e2}=0
                    Brc bf2{e2, row1, false};
                    childC1->refBrCs().emplace_back(bf2);
                    {
                        std::vector<int> i2;
                        std::vector<double> v2;
                        childC1->obtainBrcCoefficient(e2, i2, v2);
                        TestingDetail::addBranchConstraint(i2, v2,
                                                           childC1->refSolver(), /*true=*/false);
                    }
                    children.push_back(childC1);
                }

                // Child C2: enforce e1=0, e2=1
                {
                    int row2;
                    SAFE_SOLVER(node->refSolver().getNumRow(&row2));
                    Brc bf1{e1, row2, false};
                    BbNode *childC2 = new BbNode(node,
                                                 !IF_SYMMETRY_PROHIBIT,
                                                 pricing_controller.getNumBucketPerVertex(),
                                                 bf1);
                    {
                        std::vector<int> i1;
                        std::vector<double> v1;
                        childC2->obtainBrcCoefficient(e1, i1, v1);
                        TestingDetail::addBranchConstraint(i1, v1,
                                                           childC2->refSolver(), /*true=*/false);
                    }

                    Brc bf2{e2, row2, true};
                    childC2->refBrCs().emplace_back(bf2);
                    {
                        std::vector<int> i2;
                        std::vector<double> v2;
                        childC2->obtainBrcCoefficient(e2, i2, v2);
                        TestingDetail::addBranchConstraint(i2, v2,
                                                           childC2->refSolver(), /*true=*/true);
                    }
                    children.push_back(childC2);
                }
            }
        }

        // PRINT_REMIND("impose 3 branching done");
    }

    /**
     * BbNode Constructor (non-enumeration version)
     *
     * Creates a new BbNode based on the provided parent node. Copies relevant data structures,
     * sets up new forward/backward buckets if needed, and appends a new branch constraint (`bf`).
     *
     * @param node                   The parent BbNode.
     * @param if_symmetry            Whether to consider symmetry (skip backward bucket copying if true).
     * @param num_buckets_per_vertex The number of "buckets" per vertex for arcs.
     * @param bf                     The branching constraint to be added to this new node.
     */
    BbNode::BbNode(BbNode *node,
                   bool if_symmetry,
                   int num_buckets_per_vertex,
                   const Brc &bf)
    {
        // The parent is no longer the root if we are branching.
        node->if_root_node = false;
        // Assign a new, unique index to this node.
        idx = ++node_idx_counter;

        // Copy basic data from the parent node.
        cols = node->cols;
        solver.getSolver(&node->solver);
        rccs = node->rccs;
        r1cs = node->r1cs;
        brcs = node->brcs;

        // Add the new branching constraint to this node's list of constraints.
        brcs.emplace_back(bf);

        // Copy the parent node's solution value and other relevant attributes.
        value = node->value;
        num_forward_bucket_arcs = node->num_forward_bucket_arcs;
        num_forward_jump_arcs = node->num_forward_jump_arcs;
        topological_order_forward = node->topological_order_forward;

        // If symmetry is not considered, copy backward arcs as well.
        if (!if_symmetry)
        {
            topological_order_backward = node->topological_order_backward;
            num_backward_bucket_arcs = node->num_backward_bucket_arcs;
            num_backward_jump_arcs = node->num_backward_jump_arcs;
        }
        last_gap = node->last_gap;

        // Allocate and copy the forward buckets from the parent node.
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

        // If symmetry is off, also copy the backward buckets.
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

    /**
     * BbNode Constructor (enumeration version)
     *
     * Creates a new BbNode in "enumeration" state. Copies different data structures
     * tailored for column enumeration, such as the enumeration column pool and its metadata.
     *
     * @param node The parent BbNode (already in or switching to enumeration state).
     * @param bf   The branching constraint to be added to this new node.
     */
    BbNode::BbNode(BbNode *node, const Brc &bf)
    {
        // The parent is no longer the root if branching is occurring.
        node->if_root_node = false;
        // Mark this new node as in enumeration state.
        if_in_enu_state = true;
        // Assign a new, unique index to this node.
        idx = ++node_idx_counter;

        // Copy essential data from the parent node.
        rccs = node->rccs;
        r1cs = node->r1cs;
        brcs = node->brcs;
        last_gap = node->last_gap;

        // Add the new branching constraint.
        brcs.emplace_back(bf);

        // Access the solver from the parent and copy relevant data.
        solver.getSolver(&node->refSolver());
        valid_size = node->valid_size;

        // Copy column pool and cost structures for enumeration.
        index_columns_in_enumeration_column_pool = node->index_columns_in_enumeration_column_pool;
        cost_for_columns_in_enumeration_column_pool = node->cost_for_columns_in_enumeration_column_pool;

        // Copy the deleted-columns list up to the current size.
        deleted_columns_in_enumeration_pool.assign(
            node->deleted_columns_in_enumeration_pool.begin(),
            node->deleted_columns_in_enumeration_pool.begin() + node->index_columns_in_enumeration_column_pool.size());

        // Copy the current columns and node value.
        cols = node->cols;
        value = node->value;
    }
}