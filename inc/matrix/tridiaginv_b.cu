#include <iostream>
#include <iomanip>

#include "tridiaginv.h"

#include "backend/timer.h"
#include <cusp/transpose.h>
#include <cusp/print.h>
#include <cusp/array2d.h>
#include <cusp/elementwise.h>
#include <cusp/blas/blas.h>
#include "cg.h"
#include "lgmres.h"
#include "bicgstabl.h"
#include <cusp/print.h>

using value_type = double;
using memory_type = cusp::host_memory;
using CooMatrix =  cusp::coo_matrix<int, double, memory_type>;
using DiaMatrix =  cusp::dia_matrix<int, double, memory_type>;
using Container = dg::HVec;

double mu(double s, unsigned i, unsigned n) { 
    return (1.0+1.0/s*(1.0-1.0/pow(1.0 + s,n-i-1.0)));
}

int main()
{
    dg::Timer t;
    unsigned size = 50;
    std::cout << "#Specify size of vectors (50)\n";
    std::cin >> size;
    unsigned max_outer =300;
    unsigned max_inner = 300;
    unsigned restarts = 30000;
//     std::cout << "# max_outer, max_inner and restarts of lgmres (30,10,10000) \n";
//     std::cin >> max_outer >> max_inner >> restarts;
    
    std::cout << "#Constructing and filling vectors\n";
    std::vector<value_type> a(size,1.);
    std::vector<value_type> b(size,1.);
    std::vector<value_type> c(size,1.);
    std::vector<value_type> a_sym(size,1.);
    std::vector<value_type> b_sym(size,1.);
    double s= 1.1;
    for (unsigned i=0;i<size; i++)
    {
        //vectors of non-symmetric tridiagonal matrix
        a[i] = 1.0;
        b[i] = -1.0/(2.0+s);
        c[i] = -(1.0+s)/(2.0+s);
        //vectors of symmetric tridiagonal matrix
        if (i<size-1) {
            a_sym[i] = 4.0*(i+1)*(i+1)*(i+1)/(4.0*(i+1)*(i+1)-1.0);
        }
        else {
            a_sym[i] = size*size/(2.0*size-1.0);
        }        
        b_sym[i] = -1.0*(i+1)*((i+1)+1.0)/(2.0*(1+i)+1.0);
    }
    std::cout << "#Constructing and filling containers\n";
    const Container d(size,1.);
    Container x(size,0.), x_symsol(x), x_sol(x), err(x);
    std::cout << "#Constructing Matrix inversion and linear solvers\n";
    value_type eps= 1e-20;
    t.tic();
    dg::CG <Container> pcg( x,  size*size+1);
    t.toc();
    std::cout << "#Construction of CG took "<< t.diff()<<"s \n";
    t.tic();    
    dg::LGMRES <Container> lgmres( x, max_outer, max_inner, restarts);
    t.toc();
    std::cout << "#Construction of LGMRES took "<< t.diff()<<"s \n";
    t.tic();    
    dg::BICGSTABl <Container> bicg( x,size*size,4);
    t.toc();
    std::cout << "#Construction of BICGSTABl took "<< t.diff()<<"s \n";
    t.tic();
    dg::TridiagInvDF<Container, DiaMatrix, CooMatrix> tridiaginvDF(a);
    t.toc();
    std::cout << "#Construction of Tridiagonal inversion DF routine took "<< t.diff()<<"s \n";
    t.tic();
    dg::TridiagInvD<Container, DiaMatrix, CooMatrix> tridiaginvD(a);
    t.toc();
    std::cout << "#Construction of Tridiagonal inversion D routine took "<< t.diff()<<"s \n";
    
    //Create Tridiagonal and fill matrix
    DiaMatrix T, Tsym; 
    T.resize(size, size, 3*size-2, 3);
    T.diagonal_offsets[0] = -1;
    T.diagonal_offsets[1] =  0;
    T.diagonal_offsets[2] =  1;
    Tsym.resize(size, size, 3*size-2, 3);
    Tsym.diagonal_offsets[0] = -1;
    Tsym.diagonal_offsets[1] =  0;
    Tsym.diagonal_offsets[2] =  1;
    
    for( unsigned i=0; i<size-1; i++)
    {
        T.values(i,1)   =  a[i];  // 0 diagonal
        T.values(i+1,0) =  c[i];  // -1 diagonal
        T.values(i,2)   =  b[i];  // +1 diagonal //dia_rows entry works since its outside of matrix
        Tsym.values(i,1)   =  a_sym[i];  // 0 diagonal
        Tsym.values(i+1,0) =  b_sym[i];  // -1 diagonal
        Tsym.values(i,2)   =  b_sym[i];  // +1 diagonal //dia_rows entry works since its outside of matrix
    }
    T.values(size-1,1) =  a[size-1];
    Tsym.values(size-1,1) =  a_sym[size-1];
    
    //Create Inverse of tridiagonal matrix
    CooMatrix Tinv, Tsyminv, Tinv_sol, Tsyminv_sol;
    Tinv_sol.resize(size, size,  size* size);
    Tsyminv_sol.resize(size, size,  size* size);    
    for( unsigned i=0; i<size; i++) //row index
    {   
        for( unsigned j=0; j<size; j++) //column index
        {   
            Tinv_sol.row_indices[i*size+j]    = i;
            Tinv_sol.column_indices[i*size+j] = j; 
            Tsyminv_sol.row_indices[i*size+j]    = i;
            Tsyminv_sol.column_indices[i*size+j] = j; 
            if (i>= j) 
            {
                Tinv_sol.values[i*size+j] = (2.0+s)/(1.0+s)*mu(s,i+1,size+1)*mu(s,size+1-(j+1),size+1)/mu(s,0,size+1);
                Tsyminv_sol.values[i*size+j] = (j+1.0)/(i+1.0);
            }
            else
            {
                Tsyminv_sol.values[i*size+j] = (i+1.0)/(j+1.0);
            }
            
        }
    }
    for( unsigned i=0; i<size; i++) //row index
    {   
        for( unsigned j=0; j<size; j++) //column index
        {   
            if (i<j) 
            {
                Tinv_sol.values[i*size+j] = pow(1.0/(1.0+s),j-i)*Tinv_sol.values[j*size+i];
            }
            
        }
    }
    dg::blas2::gemv(Tinv_sol, d, x_sol);
    dg::blas2::gemv(Tsyminv_sol, d, x_symsol);

    //Do inversions
    std::cout << "####Compute inverse of symmetric tridiagonal matrix\n";
    std::cout << "CG:" << std::endl;
    dg::blas1::scal(x, 0.);
    t.tic();
    unsigned number = pcg( Tsym, x, d, d, eps);
    if(  number == pcg.get_max())
        throw dg::Fail( eps);
    t.toc();
    dg::blas1::axpby(1.0, x, -1.0, x_symsol, err );
    std::cout << "    time: "<< t.diff()<<"s \n";
    std::cout << "    error_rel: " << sqrt(dg::blas1::dot(err,err)/dg::blas1::dot(x_symsol,x_symsol)) << "\n";
    std::cout << "InvtridiagDF(v_sym):" << std::endl;
    t.tic();
    Tsyminv = tridiaginvDF(a_sym,b_sym,b_sym);
    t.toc();
    dg::blas2::gemv(Tsyminv, d, x);
    dg::blas1::axpby(1.0, x, -1.0, x_symsol, err );
    std::cout <<  "    time: "<< t.diff()<<"s \n";
    std::cout <<  "    error_rel: " << sqrt(dg::blas1::dot(err,err)/dg::blas1::dot(x_symsol,x_symsol)) << "\n";
    std::cout << "InvtridiagDF(Tsym):" << std::endl;
    t.tic();
    Tsyminv = tridiaginvDF(Tsym);
    t.toc();
    dg::blas2::gemv(Tsyminv, d, x);
    dg::blas1::axpby(1.0, x, -1.0, x_symsol, err );
    std::cout <<  "    time: "<< t.diff()<<"s \n";
    std::cout <<  "    error_rel: " << sqrt(dg::blas1::dot(err,err)/dg::blas1::dot(x_symsol,x_symsol)) << "\n";
    std::cout <<  "    #error_rel in T_{m,1}: " << abs(Tsyminv.values[size-1] - Tsyminv_sol.values[size-1])/abs(Tsyminv_sol.values[size-1]) << "\n";
    std::cout << "InvtridiagD(v_sym):" << std::endl;
    t.tic();
    Tsyminv = tridiaginvD(a_sym,b_sym,b_sym);
    t.toc();
    dg::blas2::gemv(Tsyminv, d, x);
    dg::blas1::axpby(1.0, x, -1.0, x_symsol, err );
    std::cout <<  "    time: "<< t.diff()<<"s \n";
    std::cout <<  "    error_rel: " << sqrt(dg::blas1::dot(err,err)/dg::blas1::dot(x_symsol,x_symsol)) << "\n";
    std::cout << "InvtridiagD(Tsym):" << std::endl;
    t.tic();
    Tsyminv = tridiaginvD(Tsym);
    t.toc();
    dg::blas2::gemv(Tsyminv, d, x);
    dg::blas1::axpby(1.0, x, -1.0, x_symsol, err );
    std::cout <<  "    time: "<< t.diff()<<"s \n";
    std::cout <<  "    error_rel: " << sqrt(dg::blas1::dot(err,err)/dg::blas1::dot(x_symsol,x_symsol)) << "\n";
    std::cout <<  "    #error_rel in T_{m,1}: " << abs(Tsyminv.values[size-1] - Tsyminv_sol.values[size-1])/abs(Tsyminv_sol.values[size-1]) << "\n";
    

    std::cout << "\n####Compute inverse of non-symmetric tridiagonal matrix\n";
    std::cout << "lGMRES:" << std::endl;
    dg::blas1::scal(x, 0.);
    t.tic();
    number = lgmres.solve( T, x, d , d, d, eps, 1);    
    t.toc();
    dg::blas1::axpby(1.0, x, -1.0, x_sol, err );
    std::cout <<  "    time: "<< t.diff()<<"s \n";
    std::cout <<  "    error_rel: " << sqrt(dg::blas1::dot(err,err)/dg::blas1::dot(x_sol,x_sol)) << "\n";  
    std::cout << "BICGSTABl:" << std::endl;

    dg::blas1::scal(x, 0.);
    t.tic();
    number = bicg.solve( T, x, d , d, d, eps, 1);    
    t.toc();
    dg::blas1::axpby(1.0, x, -1.0, x_sol, err );
    std::cout <<  "    time: "<< t.diff()<<"s \n";
    std::cout <<  "    error_rel: " << sqrt(dg::blas1::dot(err,err)/dg::blas1::dot(x_sol,x_sol)) << "\n";     
    std::cout << "InvtridiagDF(v):" << std::endl;
    t.tic();
    Tinv = tridiaginvDF(a,b,c);
    t.toc();
    dg::blas2::gemv(Tinv, d, x);
    dg::blas1::axpby(1.0, x, -1.0, x_sol, err );
    std::cout <<  "    time: "<< t.diff()<<"s \n";
    std::cout <<  "    error_rel: " << sqrt(dg::blas1::dot(err,err)/dg::blas1::dot(x_sol,x_sol)) << "\n";
    std::cout << "InvtridiagDF(T):" << std::endl;
    t.tic();
    Tinv = tridiaginvDF(T);
    t.toc();
    dg::blas2::gemv(Tinv, d, x);
    dg::blas1::axpby(1.0, x, -1.0, x_sol, err );
    std::cout <<  "    time: "<< t.diff()<<"s \n";
    std::cout <<  "    error_rel: " << sqrt(dg::blas1::dot(err,err)/dg::blas1::dot(x_sol,x_sol)) << "\n";    
    std::cout << "InvtridiagD(v):" << std::endl;
    t.tic();
    Tinv = tridiaginvD(a,b,c);
    t.toc();
    dg::blas2::gemv(Tinv, d, x);
    dg::blas1::axpby(1.0, x, -1.0, x_sol, err );
    std::cout <<  "    time: "<< t.diff()<<"s \n";
    std::cout <<  "    error_rel: " << sqrt(dg::blas1::dot(err,err)/dg::blas1::dot(x_sol,x_sol)) << "\n";
    std::cout << "InvtridiagD(T):" << std::endl;
    t.tic();
    Tinv = tridiaginvD(T);
    t.toc();
    dg::blas2::gemv(Tinv, d, x);
    dg::blas1::axpby(1.0, x, -1.0, x_sol, err );
    std::cout <<  "    time: "<< t.diff()<<"s \n";
    std::cout <<  "    error_rel: " << sqrt(dg::blas1::dot(err,err)/dg::blas1::dot(x_sol,x_sol)) << "\n";     

    return 0;
}
