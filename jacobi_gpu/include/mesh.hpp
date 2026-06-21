// mesh.hpp
#pragma once
#include <iostream>
#include <vector>
#include <iomanip>
#include <algorithm>   
#include <mpi.h>

#include "mpi_dt.hpp"
#include "printer.hpp"
#include "timer.hpp"


inline void log_ts(const char* msg) {
    using namespace std::chrono;
    auto old_fill = std::cout.fill();          
    auto n  = system_clock::now();
    auto ms = duration_cast<milliseconds>(n.time_since_epoch()) % 1000;
    std::time_t tt = system_clock::to_time_t(n);

    std::cout << msg << " || "
              << std::put_time(std::localtime(&tt), "%Y-%m-%d %H:%M:%S")
              << '.' << std::setw(3) << std::setfill('0') << ms.count()
              << '\n';

    std::cout.fill(old_fill);                  
}


template <typename T>
class CMesh{    
public:
    long int N1, N2;              // local interior rows (N1) and global interior cols (N2 == N)  
    long int n_global;            // global interior size N                                       
    std::vector<T> old_field;     // size: (N1+2)*(N2+2)
    std::vector<T> new_field;     // size: (N1+2)*(N2+2)

    long max_steps{0};

    // MPI related
    int world_rank{0}, world_size{1};                                                             

    // these depend on MPI/world and N -> not const
    long base{0}, rem{0}, num_rows{0}, i_start{0}, i_end{0};    //elements split
    long cols{0}, rows{0}; // local cols and rows (N+2)

    T corner_value; // heat source
    std::size_t size;
    
    CMesh(long int N, T corner, long steps);
    
    void apply_boundary();
    void exchange_boundaries();
    void jacobi_solver();

private:
    inline long idx(long i, long j) const noexcept { return i * cols + j; }
};


// ctor 
template <typename T>
CMesh<T>::CMesh(long int N, T corner, long steps) 
: N2(N), n_global(N), corner_value(corner), max_steps(steps)                                               
{   
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    // divide and conquer baby
    base     = n_global / world_size;
    rem      = n_global %  world_size;
    num_rows = base + ((world_rank < rem) ? 1 : 0);
    i_start  = world_rank * base + std::min<long>(world_rank, rem);
    i_end    = i_start + num_rows;

    N1 = num_rows;                                                                               

    // Allocate local grid with halos
    rows = N1 + 2;       // add top/bottom halos
    cols = N2 + 2;       // add left/right halos

    size = static_cast<std::size_t>(rows) * static_cast<std::size_t>(cols);     

    {
    CTimer t("INIT fields with Halo");
    
    old_field.assign(size, T(0.5));
    new_field.assign(size, T(0.5));
    apply_boundary();
    }
    
}





template <typename T>
void CMesh<T>::apply_boundary() {
    /*
    Applies boundary as equal linear step on all corners
    Applies it to both old_field and new_field
    */

    // Top boundary row = 0 (only rank 0)
    if (world_rank == 0) {
        for (long j = 0; j < cols; ++j) {
            old_field[idx(0, j)] = T(0);
            new_field[idx(0, j)] = T(0);
        }
    }

    // Right boundary column = 0 (all ranks)
    for (long i = 0; i < rows; ++i) {
        old_field[idx(i, cols - 1)] = T(0);
        new_field[idx(i, cols - 1)] = T(0);
    }

    // Left boundary column for INTERIOR rows only (i = 1..N1), global-consistent ramp 0..corner
    for (long i = 1; i <= N1; ++i) {
        long G = i_start + i;  // 1..n_global
        T v = corner_value * static_cast<T>(G) / static_cast<T>(n_global);  // equal steps per row
        old_field[idx(i, 0)] = v;
        new_field[idx(i, 0)] = v;
    }


    // Bottom boundary row (only last rank): corner .. 0 across columns
    if (world_rank == world_size - 1) {
        // Bottom-left corner explicitly set to corner_value
        old_field[idx(rows - 1, 0)] = corner_value;
        new_field[idx(rows - 1, 0)] = corner_value;

        // Interior columns j=1..N2 ramp corner→0; right halo (j=cols-1) already 0
        for (long j = 1; j <= N2; ++j) {
            T v = corner_value * static_cast<T>(N2 - (j - 1)) / static_cast<T>(N2);
            old_field[idx(rows - 1, j)] = v;
            new_field[idx(rows - 1, j)] = v;
        }
    }
}


// template <typename T>
// void CMesh<T>::jacobi_solver() {

//     const long local_rows = rows;
//     const long local_cols = cols;
//     const long local_N1   = N1;
//     const long local_N2   = N2; 
//     const int  local_rank = world_rank;
//     const int  local_size = world_size;
//     const long local_steps = max_steps;

//     if (local_rows <= 2 || local_cols <= 2 || local_N1 == 0 || local_steps <= 0) return;
//     if (local_rank == 0) log_ts("STARTING solver execution at rank 0");

//     const long i_last = local_rows - 1;
//     const long j_last = local_cols - 1;

//     T* tmp_old_field = old_field.data();
//     T* tmp_new_field = new_field.data();
//     size_t elem_size = local_rows * local_cols; 

//     #pragma acc data copyin(tmp_old_field[0:elem_size], tmp_new_field[0:elem_size])
//     { 
//         for (long step = 1; step <= local_steps; ++step) { 
//              { // TIMER START
//                 CTimer t("COMMUNICATION");
//             #pragma acc host_data use_device(tmp_old_field)
//             {
//                 const int rank_above = (local_rank > 0) ? local_rank - 1 : MPI_PROC_NULL;
//                 const int rank_below = (local_rank + 1 < local_size) ? local_rank + 1 : MPI_PROC_NULL;
//                 const int count = static_cast<int>(local_N2);
                
//                 T* send_up    = tmp_old_field + (1 * local_cols + 1);     
//                 T* recv_up    = tmp_old_field + (0 * local_cols + 1);     
//                 T* send_down  = tmp_old_field + (local_N1 * local_cols + 1);    
//                 T* recv_down  = tmp_old_field + ((local_rows-1) * local_cols + 1);

               
                
//                 MPI_Sendrecv(send_up, count, MPI_DOUBLE, rank_above, 100,
//                              recv_up, count, MPI_DOUBLE, rank_above, 200,
//                              MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                
//                 MPI_Sendrecv(send_down, count, MPI_DOUBLE, rank_below, 200,
//                              recv_down, count, MPI_DOUBLE, rank_below, 100,
//                              MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                
//             } 
//             } // TIMER END

//             { // TIMER START
//                 CTimer t("COMPUTATION + SWAP");
//             #pragma acc parallel loop collapse(2) present(tmp_old_field, tmp_new_field)
//             for (long i = 1; i < i_last; ++i) {
//                 for (long j = 1; j < j_last; ++j) {
                    
//                     long center = i * local_cols + j;
//                     long up     = (i-1) * local_cols + j;
//                     long down   = (i+1) * local_cols + j;
//                     long left   = i * local_cols + (j-1);
//                     long right  = i * local_cols + (j+1);

//                     tmp_new_field[center] =
//                         (  tmp_old_field[up]
//                          + tmp_old_field[left]
//                          + tmp_old_field[right]
//                          + tmp_old_field[down] ) * static_cast<T>(0.25);
//                 }
//             }
        
//             std::swap(tmp_old_field, tmp_new_field);

//             #ifdef _OPENACC
//                 #pragma acc host_data use_device(tmp_old_field, tmp_new_field)
//                 {
//                     std::swap(tmp_old_field, tmp_new_field);
//                 }
//             #endif

//             old_field.swap(new_field);
//             } // TIMER END

//         } // for loop

//         #pragma acc update host(tmp_old_field[0:elem_size])
    
//     } // data region end
       
//     if (local_rank == 0) log_ts("ENDING solver execution at rank 0");
// }

template <typename T>
void CMesh<T>::jacobi_solver() {

    const long local_rows = rows;
    const long local_cols = cols;
    const long local_N1   = N1;
    const long local_N2   = N2; 
    const int  local_rank = world_rank;
    const int  local_size = world_size;
    const long local_steps = max_steps;

    if (local_rows <= 2 || local_cols <= 2 || local_N1 == 0 || local_steps <= 0) return;
    if (local_rank == 0) log_ts("STARTING solver execution at rank 0");

    const long i_last = local_rows - 1;
    const long j_last = local_cols - 1;

    // Use two completely distinct pointer variables to make GCC happy!
    T* ptr_old = old_field.data();
    T* ptr_new = new_field.data();
    size_t elem_size = local_rows * local_cols; 

    // Now GCC sees two separate variables in the copyin clause
    #pragma acc data copyin(ptr_old[0:elem_size], ptr_new[0:elem_size])
    { 
        for (long step = 1; step <= local_steps; ++step) { 
            
            // Toggle logic using the ternary operator
            // If step is odd, d_old is ptr_old. If even, d_old is ptr_new.
            T* d_old = (step % 2 != 0) ? ptr_old : ptr_new;
            T* d_new = (step % 2 != 0) ? ptr_new : ptr_old;
            
            #pragma acc host_data use_device(d_old)
            {
                const int rank_above = (local_rank > 0) ? local_rank - 1 : MPI_PROC_NULL;
                const int rank_below = (local_rank + 1 < local_size) ? local_rank + 1 : MPI_PROC_NULL;
                const int count = static_cast<int>(local_N2);
                
                T* send_up    = d_old + (1 * local_cols + 1);     
                T* recv_up    = d_old + (0 * local_cols + 1);     
                T* send_down  = d_old + (local_N1 * local_cols + 1);    
                T* recv_down  = d_old + ((local_rows-1) * local_cols + 1);

                { // TIMER START
                CTimer t("COMMUNICATION");
                MPI_Sendrecv(send_up, count, MPI_DOUBLE, rank_above, 100,
                             recv_up, count, MPI_DOUBLE, rank_above, 200,
                             MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                
                MPI_Sendrecv(send_down, count, MPI_DOUBLE, rank_below, 200,
                             recv_down, count, MPI_DOUBLE, rank_below, 100,
                             MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                } // TIMER END
            } 
            
            { // TIMER START
            CTimer t("COMPUTATION");
            #pragma acc parallel loop collapse(2) present(d_old, d_new)
            for (long i = 1; i < i_last; ++i) {
                for (long j = 1; j < j_last; ++j) {
                    
                    long center = i * local_cols + j;
                    long up     = (i-1) * local_cols + j;
                    long down   = (i+1) * local_cols + j;
                    long left   = i * local_cols + (j-1);
                    long right  = i * local_cols + (j+1);

                    d_new[center] =
                        (  d_old[up]
                         + d_old[left]
                         + d_old[right]
                         + d_old[down] ) * static_cast<T>(0.25);
                }
            }
            // Sync before stopping the computation timer
            #pragma acc wait
            } // TIMER END

        } // for loop

        // Bring the correct buffer back to the host
        if (local_steps % 2 != 0) {
            #pragma acc update host(ptr_new[0:elem_size])
        } else {
            #pragma acc update host(ptr_old[0:elem_size])
        }
    
    } // data region end
       
    // Sync the host std::vectors if we ended on an odd step
    if (local_steps % 2 != 0) {
        old_field.swap(new_field);
    }

    if (local_rank == 0) log_ts("ENDING solver execution at rank 0");
}