#include <iostream>
#include <iomanip>

#include "lanczos.h"

#include "backend/timer.h"
#include <cusp/transpose.h>
#include <cusp/print.h>
#include <cusp/array2d.h>
#include <cusp/elementwise.h>
#include <cusp/blas/blas.h>
using memory_type = cusp::host_memory;
using CooMatrix =  cusp::coo_matrix<int, double, memory_type>;
using DiaMatrix =  cusp::dia_matrix<int, double, memory_type>;
using Container = dg::HVec;
int main()
{
    dg::Timer t;
    std::vector<double> a = {1.98242,      4.45423,     5.31867,    7.48144,  7.11534};
    std::vector<double> b = {-0.00710891, -0.054661, -0.0554193, -0.0172191, -0.297645};
    std::vector<double> c = {-1.98242,     -4.44712,   -5.26401,   -7.42602, -7.09812}; 
    unsigned size = a.size();

    DiaMatrix T; 
    CooMatrix Tinv, Tinv_sol, Tinv_error;
    cusp::array2d<double ,memory_type> H(size,size), error(size,size,0.);
    H(0,0) = 0.505249;  H(0,1) = 0.000814795;  H(0,2) =  8.4358e-6;     H(0,3) = 6.26392e-8;     H(0,4) =  1.51587e-10;
    H(1,0) = 0.227217;  H(1,1) = 0.227217;     H(1,2) =  0.00235244;    H(1,3) = 0.0000174678;   H(1,4) =  4.22721e-8;
    H(2,0) = 0.19139;   H(2,1) = 0.19139;      H(2,2) =  0.19139;       H(2,3) = 0.00142115;     H(2,4) =  3.43918e-6;
    H(3,0) = 0.134988;  H(3,1) = 0.134988;     H(3,2) =  0.134988;      H(3,3) = 0.134988;       H(3,4) =  0.000326671;
    H(4,0) = 0.140882;  H(4,1) = 0.140882;     H(4,2) =  0.140882;      H(4,3) = 0.140882;       H(4,4) = 0.140882;
    cusp::convert(H,Tinv_sol);
    cusp::convert(error,Tinv_error);
    
    T.resize(size, size, 3*size-2, 3);
    T.diagonal_offsets[0] = -1;
    T.diagonal_offsets[1] =  0;
    T.diagonal_offsets[2] =  1;
    for( unsigned i=0; i<size-1; i++)
    {
        T.values(i,1)   =  a[i];  // 0 diagonal
        T.values(i+1,0) =  c[i];  // -1 diagonal
        T.values(i,2)   =  b[i];  // +1 diagonal //dia_rows entry works since its outside of matrix
    }
    T.values(size-1,1) =  a[size-1];
    std::cout << "T matrix\n";
    cusp::print(T);

    dg::InvTridiag<Container, DiaMatrix, CooMatrix> invtridiag(a);
    t.tic();
    Tinv = invtridiag(a,b,c);
    t.toc();
    std::cout << "Difference between Tinv and solution\n";
    cusp::subtract(Tinv_sol, Tinv, Tinv_error);
    cusp::print(Tinv_error);
    std::cout <<  "time: "<< t.diff()<<"s \n";
    
    t.tic();
    Tinv = invtridiag(T);
    t.toc();

    std::cout << "Difference between Tinv and solution\n";
    cusp::subtract(Tinv_sol, Tinv, Tinv_error);
    cusp::print(Tinv_error);
    std::cout <<  "time: "<< t.diff()<<"s \n";
    return 0;
}
