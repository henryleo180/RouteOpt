/* 
 * Copyright (c) 2025 Zhengzhong (Ricky) You.
 * All rights reserved.
 * Software: RouteOpt
 * License: GPL-3.0
 */

#include <numeric>
#include "cvrp.hpp"

namespace RouteOpt::Application::CVRP {
    namespace ImposeBranchingDetail {
        inline void deleteArcByFalseBranchConstraint(int num_buckets_per_vertex, Bucket **buckets,
                                                     const std::pair<int, int> &edge) {
            int i = edge.first, j = edge.second;
            bool if_rep = false;
        AGAIN:
            for (int bin = 0; bin < num_buckets_per_vertex; ++bin) {
                auto if_find = std::find(buckets[i][bin].bucket_arcs.begin(),
                                         buckets[i][bin].bucket_arcs.end(), j);
                if (if_find == buckets[i][bin].bucket_arcs.end()) {
                    auto iff = std::find_if(buckets[i][bin].jump_arcs.begin(),
                                            buckets[i][bin].jump_arcs.end(),
                                            [&](const std::pair<res_int, int> &p) { return p.second == j; });
                    if (iff != buckets[i][bin].jump_arcs.end()) {
                        buckets[i][bin].jump_arcs.erase(iff);
                    }
                } else {
                    buckets[i][bin].bucket_arcs.erase(if_find);
                }
            }
            if (!if_rep) {
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
            bool if_symmetry
        )
        {
            // If this node is flagged to terminate (e.g. infeasible or solver stop condition),
            // then do nothing and return immediately.
            if (node->getIfTerminate()) return;

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
            if (solver_ind.front() == 0) {
                solver_ind.erase(solver_ind.begin());
            }

            // Remove these columns from the parent's LP to enforce the "false" branch.
            node->rmLPCols(solver_ind);

            // Delete (or mark infeasible) any forward arcs corresponding to the "false" branch.
            deleteArcByFalseBranchConstraint(num_buckets_per_vertex,
                                            node->refAllForwardBuckets(),
                                            info);

            // If symmetry is disabled, also remove corresponding backward arcs.
            if (!if_symmetry) {
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
                                        int *col_pool4_pricing) {
            // If this node is flagged to terminate, do nothing and return immediately.
            if (node->getIfTerminate()) return;

            // Prepare vectors to hold indices and coefficients for the branching constraint.
            std::vector<int> ind_use;
            std::vector<double> solver_val;
            // Obtain the coefficients for the branch from the node, based on 'info'.
            node->obtainBrcCoefficient(info, ind_use, solver_val);

            // If the first index is 0, remove it. Possibly represents a special offset or dummy column.
            if (ind_use.front() == 0) {
                ind_use.erase(ind_use.begin());
            }

            // Obtain the set of column indices that are not allowed for this branch.
            std::vector<int> ind_not_allow;
            node->obtainColIdxNotAllowByEdge(info, ind_not_allow);

            // Similar check for a leading 0 index in 'ind_not_allow'.
            if (ind_not_allow.front() == 0) {
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
            bf.br_dir = true;              // "true" branch direction
            bf.idx_brc = INVALID_BRC_INDEX; // No row index used here for enumeration state

            // ------------------------------------------------------
            // TRUE BRANCH: Create a child node that enforces the 'true' constraint
            // ------------------------------------------------------
            node2 = new BbNode(node, bf);

            // Remove columns in the new child node that are "not allowed" for this "true" branch.
            node2->rmLPCols(ind_not_allow);

            // If the parent node uses an enumeration column pool, remove columns and regenerate the matrix.
            if (node->refIndexColumnsInEnumerationColumnPool().size() != 0) {
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
            if (node->refIndexColumnsInEnumerationColumnPool().size() != 0) {
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
    void CVRPSolver::imposeBranching(BbNode *node, const std::pair<int, int> &brc, std::vector<BbNode *> &children) {
        // Prepare to store 2 children in the vector. 
        // The first child is initialized to point to the same node (often a way to handle "false" branch in-place).
        children.resize(2);
        children.front() = node;

        // Decide which branching function to use based on enumeration state.
        // If in enumeration state, call 'addBranchCutToUnsolvedInEnu';
        // otherwise, call the regular 'addBranchCutToUnsolved'.
        if (node->getIfInEnumState()) {
            ImposeBranchingDetail::addBranchCutToUnsolvedInEnu(
                node,
                children.back(),             // This is where the "true" branch is stored
                brc,
                pricing_controller.getColumnPoolPtr()
            );
        } else {
            ImposeBranchingDetail::addBranchCutToUnsolved(
                node,
                children.back(),             // This is where the "true" branch is stored
                brc,
                pricing_controller.getNumBucketPerVertex(),
                !IF_SYMMETRY_PROHIBIT        // A flag indicating whether to allow symmetry
            );
        }

        // If the second child (children.back()) was not created (nullptr), remove it from the vector.
        if (children.back() == nullptr) children.pop_back();

        // Clear or reset some data related to edges in the original node (likely to finalize the branching step).
        node->clearEdgeMap();
    };

    void CVRPSolver::imposeThreeBranching(
        BbNode* node,
        const std::pair<int,int>& edge1,
        const std::pair<int,int>& edge2,
        std::vector<BbNode*>& children
    ) {
        // // 1. Prepare space for up to 3 children.
        // //    (You may store them in children[0], children[1], children[2] if feasible).
        // children.resize(3, nullptr);

        // // 2. If node is flagged to terminate or is infeasible, just return.
        // if (node->getIfTerminate()) return;

        // // 3. Possibly do some checks to see if 3-branch is allowed or beneficial.
        // //    For example, if edge1 + edge2 = 1.0 in the LP solution already
        // //    (and that leads to loops or confusion), you might skip 3-branch.

        // // 4. Create the "both accepted" child
        // //    - This is the branch a+b >= 2 in your model.
        // //    - Implementation: impose constraints that x_edge1 = 1, x_edge2 = 1, or however "accept" is enforced.

        // BbNode* bothAcceptedNode = createChildNodeForBothAccepted(node, edge1, edge2);
        // if (bothAcceptedNode != nullptr) {
        //     children[0] = bothAcceptedNode;
        // }

        // // 5. Create the "both rejected" child
        // //    - This is the branch a+b <= 0 in your model.
        // //    - Implementation: impose constraints that x_edge1 = 0, x_edge2 = 0, or however "reject" is enforced.

        // BbNode* bothRejectedNode = createChildNodeForBothRejected(node, edge1, edge2);
        // if (bothRejectedNode != nullptr) {
        //     children[1] = bothRejectedNode;
        // }

        // // 6. Create the "exactly one accepted" child
        // //    - This is the branch a+b = 1 in your model.
        // //    - Implementation: impose constraints that x_edge1 + x_edge2 = 1

        // BbNode* exactlyOneNode = createChildNodeForExactlyOne(node, edge1, edge2);
        // if (exactlyOneNode != nullptr) {
        //     children[2] = exactlyOneNode;
        // }

        // // 7. Clear or update any data in the original node if needed (similar to your 2-branch code).
        // node->clearEdgeMap();

        // // 8. If any of the newly created children are infeasible or invalid, set them to nullptr and/or shrink the vector.
        // //    For example:
        // auto it = std::remove(children.begin(), children.end(), nullptr);
        // children.erase(it, children.end());
        
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
                const Brc &bf) {
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
        if (!if_symmetry) {
            topological_order_backward = node->topological_order_backward;
            num_backward_bucket_arcs = node->num_backward_bucket_arcs;
            num_backward_jump_arcs = node->num_backward_jump_arcs;
        }
        last_gap = node->last_gap;

        // Allocate and copy the forward buckets from the parent node.
        all_forward_buckets = new Bucket *[dim];
        if (node->all_forward_buckets == nullptr) std::terminate();
        for (int i = 0; i < dim; ++i) {
            all_forward_buckets[i] = new Bucket[num_buckets_per_vertex];
            for (int j = 0; j < num_buckets_per_vertex; ++j) {
                all_forward_buckets[i][j] = node->all_forward_buckets[i][j];
            }
        }

        // If symmetry is off, also copy the backward buckets.
        if (!if_symmetry) {
            if (node->all_backward_buckets == nullptr) std::terminate();
            all_backward_buckets = new Bucket *[dim];
            for (int i = 0; i < dim; ++i) {
                all_backward_buckets[i] = new Bucket[num_buckets_per_vertex];
                for (int j = 0; j < num_buckets_per_vertex; ++j) {
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
    BbNode::BbNode(BbNode *node, const Brc &bf) {
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
            node->deleted_columns_in_enumeration_pool.begin() + node->index_columns_in_enumeration_column_pool.size()
        );

        // Copy the current columns and node value.
        cols = node->cols;
        value = node->value;
    }
}