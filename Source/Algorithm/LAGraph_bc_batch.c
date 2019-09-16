//------------------------------------------------------------------------------
// LAGraph_bc: Brandes' algorithm for computing betweeness centrality
//------------------------------------------------------------------------------

/*
    LAGraph:  graph algorithms based on GraphBLAS

    Copyright 2019 LAGraph Contributors.

    (see Contributors.txt for a full list of Contributors; see
    ContributionInstructions.txt for information on how you can Contribute to
    this project).

    All Rights Reserved.

    NO WARRANTY. THIS MATERIAL IS FURNISHED ON AN "AS-IS" BASIS. THE LAGRAPH
    CONTRIBUTORS MAKE NO WARRANTIES OF ANY KIND, EITHER EXPRESSED OR IMPLIED,
    AS TO ANY MATTER INCLUDING, BUT NOT LIMITED TO, WARRANTY OF FITNESS FOR
    PURPOSE OR MERCHANTABILITY, EXCLUSIVITY, OR RESULTS OBTAINED FROM USE OF
    THE MATERIAL. THE CONTRIBUTORS DO NOT MAKE ANY WARRANTY OF ANY KIND WITH
    RESPECT TO FREEDOM FROM PATENT, TRADEMARK, OR COPYRIGHT INFRINGEMENT.

    Released under a BSD license, please see the LICENSE file distributed with
    this Software or contact permission@sei.cmu.edu for full terms.

    Created, in part, with funding and support from the United States
    Government.  (see Acknowledgments.txt file).

    This program includes and/or can make use of certain third party source
    code, object code, documentation and other files ("Third Party Software").
    See LICENSE file for more details.

*/

//------------------------------------------------------------------------------

// LAGraph_bc_batch: Batch algorithm for computing betweeness centrality.
// Contributed by Scott Kolodziej and Tim Davis, Texas A&M University.
// Adapted from GraphBLAS C API Spec, Appendix B.4.

// LAGraph_bc_batch computes an approximation of the betweenness centrality of
// all nodes in a graph using a batched version of Brandes' algorithm. 
//                               ____     
//                               \      sigma(s,t | i)
//    Betweenness centrality =    \    ----------------
//           of node i            /       sigma(s,t)
//                               /___ 
//                             s ≠ i ≠ t
//
// Where sigma(s,t) is the total number of shortest paths from node s to
// node t, and sigma(s,t | i) is the total number of shortest paths from
// node s to node t that pass through node i.
//
// Note that the true betweenness centrality requires computing shortest paths
// from all nodes s to all nodes t (or all-pairs shortest paths), which can be 
// expensive to compute. By using a reasonably sized subset of source nodes, an
// approximation can be made.
//
// LAGraph_bc_batch performs simultaneous breadth-first searches of the entire
// graph starting at a given set of source nodes. This pass discovers all
// shortest paths from the source nodes to all other nodes in the graph. After
// the BFS is complete, the number of shortest paths that pass through a given
// node is tallied by reversing the traversal. From this, the (approximate)
// betweenness centrality is computed.

//------------------------------------------------------------------------------

#include "LAGraph_internal.h"

#define LAGRAPH_FREE_ALL            \
{                                   \
    GrB_free (&frontier);           \
    GrB_free (&paths);              \
    GrB_free (&inv_paths);             \
    GrB_free (&bc_update);          \
    GrB_free (&desc_tsr);           \
    GrB_free (&replace);            \
    GrB_free (&temp);               \
    for (int i = 0; i < depth; i++) \
    {                               \
        GrB_free(&(S_array[i]));    \
    }                               \
    free (S_array);                 \
}

GrB_Info LAGraph_bc_batch // betweeness centrality, batch algorithm
(
    GrB_Vector *centrality,    // centrality(i) is the betweeness centrality of node i
    const GrB_Matrix A_matrix, // input graph, treated as if boolean in semiring
    const GrB_Index *sources,  // source vertices from which to compute shortest paths
    const int32_t num_sources  // number of source vertices (length of s)
)
{
    GrB_Info info;

    GrB_Index n; // Number of nodes in the graph

    // Array of BFS search matrices
    // S_array[i] is a matrix that stores the depth at which each vertex is 
    // first seen thus far in each BFS at the current depth i. Each column
    // corresponds to a BFS traversal starting from a source node.
    GrB_Matrix *S_array;

    // Frontier matrix
    // Stores # of shortest paths to vertices at current BFS depth
    GrB_Matrix frontier;
    
    // Paths matrix holds the number of shortest paths for each node and 
    // starting node discovered so far.
    GrB_Matrix paths;

    // Inverse of the number of shortest paths for each node and source node
    GrB_Matrix inv_paths;

    // Update matrix for betweenness centrality, values for each node for
    // each starting node
    GrB_Matrix bc_update;

    // Temporary workspace matrix
    GrB_Matrix temp;

    GrB_Descriptor desc_tsr;
    GrB_Descriptor replace;

    int64_t depth = 0; // Initial BFS depth

    LAGRAPH_OK (GrB_Matrix_nrows(&n, A_matrix)); // Get dimensions

    // Create the result vector, one entry for each node
    LAGRAPH_OK (GrB_Vector_new(centrality, GrB_FP64, n));
    
    // Create a new descriptor that represents the following traits:
    //  - Tranpose the first input matrix
    //  - Replace the output 
    //  - Use the structural complement of the mask
    LAGRAPH_OK (GrB_Descriptor_new(&desc_tsr));
    LAGRAPH_OK (GrB_Descriptor_set(desc_tsr, GrB_INP0, GrB_TRAN));
    LAGRAPH_OK (GrB_Descriptor_set(desc_tsr, GrB_OUTP, GrB_REPLACE));
    LAGRAPH_OK (GrB_Descriptor_set(desc_tsr, GrB_MASK, GrB_SCMP));

    // Initialize paths to source vertices with ones
    // paths[s[i],i]=1 for i=[0, ..., nsver)
    GrB_Index *i_nsver = (GrB_Index*) malloc(sizeof(GrB_Index)*num_sources);
    int64_t *ones = (int64_t*) malloc(sizeof(int64_t)*num_sources);
    for (int i = 0; i < num_sources; ++i)
    {
        i_nsver[i] = i;
        ones[i] = 1;
    }

    LAGRAPH_OK (GrB_Matrix_new(&paths, GrB_INT64, n, num_sources));

    // If sources is GrB_ALL, then we compute all-pairs shortest paths
    if (sources == GrB_ALL)
    {
        GrB_Index *s_all = malloc(sizeof(GrB_Index) * n);
        
        for (int i = 0; i < n; i++)
        {
            s_all[i] = i;
        }

        LAGRAPH_OK (GrB_Matrix_build(paths, s_all, i_nsver, ones, num_sources, GrB_PLUS_INT64));

        free(s_all);
    }
    else
    {
        // If sources is not GrB_ALL, use the provided list of source nodes
        LAGRAPH_OK (GrB_Matrix_build(paths, sources, i_nsver, ones, num_sources, GrB_PLUS_INT64));
    }

    free(i_nsver);
    free(ones);

    // Create frontier matrix and initialize to outgoing nodes from 
    // all source nodes
    LAGRAPH_OK (GrB_Matrix_new(&frontier, GrB_INT64, n, num_sources));
    LAGRAPH_OK (GrB_extract(frontier, paths, GrB_NULL, A_matrix, GrB_ALL, n, sources, num_sources, desc_tsr));

    // Allocate memory for the array of S matrices
    S_array = (GrB_Matrix*) malloc(sizeof(GrB_Matrix)*n);

    // === Breadth-first search stage ==========================================
    GrB_Index sum = 0;    // Sum of shortest paths to vertices at current depth
                          // Equal to sum(frontier). Continue BFS until new paths
                          //  are no shorter than any existing paths.
    do
    {
        // Create the current search matrix - one column for each source/BFS
        LAGRAPH_OK (GrB_Matrix_new(&(S_array[depth]), GrB_BOOL, n, num_sources));
        
        // Copy the current frontier to S
        LAGRAPH_OK (GrB_apply(S_array[depth], GrB_NULL, GrB_NULL, GrB_IDENTITY_BOOL, frontier, GrB_NULL));

        // Accumulate path counts: paths += frontier
        LAGRAPH_OK (GrB_eWiseAdd(paths, GrB_NULL, GrB_NULL, GxB_PLUS_TIMES_INT64, paths, frontier, GrB_NULL));

        // Update frontier: frontier<!paths>=A’ +.∗ frontier
        LAGRAPH_OK (GrB_mxm(frontier, paths, GrB_NULL, GxB_PLUS_TIMES_INT64, A_matrix, frontier, desc_tsr));

        // Sum up the number of BFS paths still being explored
        LAGRAPH_OK (GrB_Matrix_nvals(&sum, frontier));

        depth = depth + 1;

    } while (sum); // Repeat until no more shortest paths being discovered

    // === Betweenness centrality computation phase ============================

    // Create inverse paths matrix: inv_paths = 1 ./ paths
    LAGRAPH_OK (GrB_Matrix_new(&inv_paths, GrB_FP64, n, num_sources));
    LAGRAPH_OK (GrB_apply(inv_paths, GrB_NULL, GrB_NULL, GrB_MINV_FP64, paths, GrB_NULL));

    // Create the update matrix and initialize it to 1
    // TODO: "To avoid sparsity issues"?
    LAGRAPH_OK (GrB_Matrix_new(&bc_update, GrB_FP64, n, num_sources));
    LAGRAPH_OK (GrB_assign(bc_update, GrB_NULL, GrB_NULL, 1.0f, GrB_ALL, n, GrB_ALL, num_sources, GrB_NULL));

    // Create a descriptor to denote total replacement of the output
    // Note that this is only useful when using a mask
    LAGRAPH_OK (GrB_Descriptor_new(&replace));
    LAGRAPH_OK (GrB_Descriptor_set(replace, GrB_OUTP, GrB_REPLACE));

    // Create temporary workspace matrix
    LAGRAPH_OK (GrB_Matrix_new(&temp, GrB_FP64, n, num_sources));
    
    // Backtrack through the BFS and compute centrality updates for each vertex
    for (int i = depth - 1; i > 0; i--)
    {
        // Add contributions by successors and mask with that BFS level’s frontier
        
        // temp<S_array[i]>=(1 ./ nsp) .∗ bc_update
        LAGRAPH_OK (GrB_eWiseMult(temp, S_array[i], GrB_NULL, GxB_PLUS_TIMES_FP64, bc_update, inv_paths, replace));

        // temp<S_array[i−1]> = (A ∗ temp)
        LAGRAPH_OK (GrB_mxm(temp, S_array[i-1], GrB_NULL, GxB_PLUS_TIMES_FP64, A_matrix, temp, replace));

        // bc_update += temp .∗ paths
        LAGRAPH_OK (GrB_eWiseMult(bc_update, GrB_NULL, GrB_PLUS_FP64, GxB_TIMES_FP64_MONOID, temp, paths, GrB_NULL));
    }

    // Initialize the centrality array with -(num_sources) to avoid counting
    // zero length paths
    LAGRAPH_OK (GrB_assign(*centrality, GrB_NULL, GrB_NULL, -(float)num_sources, GrB_ALL, n, GrB_NULL));

    // centrality += update
    LAGRAPH_OK (GrB_reduce(*centrality, GrB_NULL, GrB_PLUS_FP64, GrB_PLUS_FP64, bc_update, GrB_NULL));

    LAGRAPH_FREE_ALL;
    return GrB_SUCCESS;
}
