#include <iostream>
#include <iomanip>
#include <cmath>

#include <thrust/host_vector.h>
#include <thrust/device_vector.h>

#include "dg/blas.h"
#include "dg/functors.h"

#include "evaluation.h"
#include "weights.h"

struct exp_function{
DG_DEVICE
double operator()( double x)
{
    return exp(x);
}
};
struct sin_function{
DG_DEVICE
double operator()( double x)
{
    return sin(x);
}
};
template<class T>
T function(T x, T y)
{
    T rho = 0.20943951023931953; //pi/15
    T delta = 0.050000000000000003;
    if( y<= M_PI)
        return delta*cos(x) - 1./rho/cosh( (y-M_PI/2.)/rho)/cosh( (y-M_PI/2.)/rho);
    return delta*cos(x) + 1./rho/cosh( (3.*M_PI/2.-y)/rho)/cosh( (3.*M_PI/2.-y)/rho);
}
double function3d( double x, double y, double z)
{
        return exp(x)*exp(y)*exp(z);
}


int main()
{
    std::cout << "This program tests the exblas::dot function. The tests succeed only if the evaluation and grid functions but also the weights and especially the exblas::dot function are correctly implemented and compiled. Furthermore, the compiler implementation of the exp function in the math library must be consistent across platforms to get reproducible results\n";
    std::cout << "A TEST is PASSED if the number in the second column shows EXACTLY 0!\n";
    unsigned n = 3, Nx = 12, Ny = 28, Nz = 100;
    std::cout << "On Grid "<<n<<" x "<<Nx<<" x "<<Ny<<" x "<<Nz<<"\n";

    dg::Grid1d g1d( 1, 2, n, 12);
    dg::Grid2d g2d( 0.0, 6.2831853071795862, 0.0, 6.2831853071795862, 3, 48, 48);
    dg::RealGrid2d<float> gf2d( 0.0, 6.2831853071795862, 0.0, 6.2831853071795862, 3, 48, 48);
    dg::Grid3d g3d( 1, 2, 3, 4, 5, 6, n, Nx, Ny, Nz,dg::PER,dg::PER,dg::PER);

    //test evaluation functions
    const dg::DVec func1d = dg::construct<dg::DVec>( dg::evaluate( exp, g1d));
    const dg::DVec func2d = dg::construct<dg::DVec>( dg::evaluate( function<double>, g2d));
    const dg::fDVec funcf2d = dg::construct<dg::fDVec>( dg::evaluate( function<float>, gf2d));
    const dg::DVec func3d = dg::construct<dg::DVec>( dg::evaluate( function3d, g3d));
    const dg::DVec w1d = dg::construct<dg::DVec>( dg::create::weights( g1d));
    const dg::DVec w2d = dg::construct<dg::DVec>( dg::create::weights( g2d));
    const dg::fDVec wf2d = dg::construct<dg::fDVec>( dg::create::weights( gf2d));
    const dg::DVec w3d = dg::construct<dg::DVec>( dg::create::weights( g3d));
    dg::exblas::udouble res;

    double integral = dg::blas1::dot( w1d, func1d); res.d = integral;
    std::cout << "1D integral               "<<std::setw(6)<<integral <<"\t" << res.i - 4616944842743393935  << "\n";
    double sol = (exp(2.) -exp(1));
    std::cout << "Correct integral is       "<<std::setw(6)<<sol<<std::endl;
    std::cout << "Relative 1d error is      "<<(integral-sol)/sol<<"\n\n";

    double integral2d = dg::blas1::dot( w2d, func2d); res.d = integral2d;
    std::cout << "2D integral               "<<std::setw(6)<<integral2d <<"\t" << res.i + 4823280491526356992<< "\n";
    double sol2d = 0;
    std::cout << "Correct integral is       "<<std::setw(6)<<sol2d<<std::endl;
    std::cout << "2d error is               "<<(integral2d-sol2d)<<"\n\n";

    float integralf2d = dg::blas1::dot( wf2d, funcf2d); res.d = integralf2d;
    std::cout << "2D integral (float)       "<<std::setw(6)<<integralf2d <<"\n";
    float solf2d = 0;
    std::cout << "Correct integral is       "<<std::setw(6)<<solf2d<<std::endl;
    std::cout << "2d error (float)          "<<(integralf2d-solf2d)<<"\n\n";

    double integral3d = dg::blas1::dot( w3d, func3d); res.d = integral3d;
    std::cout << "3D integral               "<<std::setw(6)<<integral3d <<"\t" << res.i - 4675882723962622631<< "\n";
    double sol3d = (exp(2.)-exp(1))*(exp(4.)-exp(3))*(exp(6.)-exp(5));
    std::cout << "Correct integral is       "<<std::setw(6)<<sol3d<<std::endl;
    std::cout << "Relative 3d error is      "<<(integral3d-sol3d)/sol3d<<"\n\n";

    double norm = dg::blas2::dot( func1d, w1d, func1d); res.d = norm;
    std::cout << "Square normalized 1D norm "<<std::setw(6)<<norm<<"\t" << res.i - 4627337306989890294 <<"\n";
    double solution = (exp(4.) -exp(2))/2.;
    std::cout << "Correct square norm is    "<<std::setw(6)<<solution<<std::endl;
    std::cout << "Relative 1d error is      "<<(norm-solution)/solution<<"\n\n";

    double norm2d = dg::blas2::dot( w2d, func2d); res.d = norm2d;
    std::cout << "Square normalized 2D norm "<<std::setw(6)<<norm2d<<"\t" << res.i - 4635333359953759707<<"\n";
    double solution2d = 80.0489;
    std::cout << "Correct square norm is    "<<std::setw(6)<<solution2d<<std::endl;
    std::cout << "Relative 2d error is      "<<(norm2d-solution2d)/solution2d<<"\n\n";

    double norm3d = dg::blas2::dot( func3d, w3d, func3d); res.d = norm3d;
    std::cout << "Square normalized 3D norm "<<std::setw(6)<<norm3d<<"\t" << res.i - 4746764681002108278<<"\n";
    double solution3d = (exp(4.)-exp(2))/2.*(exp(8.)-exp(6.))/2.*(exp(12.)-exp(10))/2.;
    std::cout << "Correct square norm is    "<<std::setw(6)<<solution3d<<std::endl;
    std::cout << "Relative 3d error is      "<<(norm3d-solution3d)/solution3d<<"\n\n";

    std::cout << "TEST result of a sin and exp function to compare compiler specific math libraries:\n";
    dg::DVec x(1, 6.12610567450009658);
    dg::blas1::transform( x, x, sin_function() );
    res.d = x[0];
    std::cout << "Result of sin:    "<<res.i<<"\n"
              << "          GCC:    -4628567870976535683 (correct)"<<std::endl;
    dg::DVec y(1, 5.9126151457310376);
    dg::blas1::transform( y, y, exp_function() );
    res.d = y[0];
    std::cout << "Result of exp:     "<<res.i<<"\n"
              << "          GCC:     4645210948416067678 (correct)"<<std::endl;

    //TEST OF INTEGRAL
    dg::HVec integral_num = dg::integrate( cos, g1d);
    dg::HVec integral_ana = dg::evaluate( sin, g1d);
    dg::blas1::plus( integral_ana, -sin(g1d.x0()));
    dg::blas1::axpby( 1., integral_ana, -1., integral_num);
    norm = dg::blas2::dot( integral_num, dg::create::weights( g1d), integral_num);
    std::cout << " Error norm of  1d integral function "<<norm<<"\n";
    // TEST if dot throws on NaN
    std::cout << "TEST if dot throws on Inf or Nan:\n";
    dg::blas1::transform( x,x, dg::LN<double>());
    thrust::device_vector<bool> boolvec ( 100, false);
    dg::blas1::transform( x, boolvec, dg::ISNFINITE<double>());
    bool hasnan = dg::blas1::reduce( boolvec, false, thrust::logical_or<bool>());
    std::cout << "x contains Inf or Nan numbers "<<std::boolalpha<<hasnan<<"\n";
    try{
        dg::blas1::dot( x,x);
    }catch ( std::exception& e)
    {
        std::cerr << "Error thrown as expected\n";
        //std::cerr << e.what() << std::endl;
    }
    std::cout << "Test MinMod function:\n";
    dg::MinMod minmod;
    std::cout << " 3 -5 ="<<minmod( 3,-5)<< " (0) "<<std::endl;
    std::cout << " 2 4 1 ="<<minmod( 2,4,1)<< " (1) "<<std::endl;
    std::cout << " 0 1 2 ="<<minmod( 0,1,2)<< " (0) "<<std::endl;
    std::cout << " -1 1 2 ="<<minmod( -1,1,2)<< " (0) "<<std::endl;
    std::cout << " -5 -3 -2 ="<<minmod( -5,-3,-2)<< " (-2) "<<std::endl;

    std::cout << "\nFINISHED! Continue with topology/derivatives_t.cu !\n\n";
    return 0;
}
