#include <iostream>
#include <iomanip>
#include <cmath>

#include <thrust/host_vector.h>
#include <thrust/device_vector.h>

#include "dg/blas.h"
#include "dg/functors.h"

#include "evaluation.h"
#include "weights.h"

#include "catch2/catch.hpp"

template<class T>
T function(T x, T y)
{
    T rho = 0.20943951023931953; //pi/15
    T delta = 0.050000000000000003;
    if( y<= M_PI)
        return delta*cos(x) - 1./rho/cosh( (y-M_PI/2.)/rho)/cosh( (y-M_PI/2.)/rho);
    return delta*cos(x) + 1./rho/cosh( (3.*M_PI/2.-y)/rho)/cosh( (3.*M_PI/2.-y)/rho);
}
static double function3d( double x, double y, double z)
{
    return exp(x)*exp(y)*exp(z);
}


TEST_CASE( "Evaluation")
{
    //This program tests the exblas::dot function. The tests succeed only if
    //the evaluation and grid functions but also the weights and especially the
    //exblas::dot function are correctly implemented and compiled. Furthermore,
    //the compiler implementation of the exp function in the math library must
    //be consistent across platforms to get reproducible results
    SECTION( "1d grid")
    {
        INFO("On Grid 3 x 12");
        dg::Grid1d g1d( 1, 2, 3, 12);
        const dg::DVec func1d = dg::construct<dg::DVec>( dg::evaluate( exp,
                    g1d));
        const dg::DVec w1d = dg::construct<dg::DVec>( dg::create::weights( g1d));
        dg::exblas::udouble res;
        double integral = dg::blas1::dot( w1d, func1d); res.d = integral;
        double sol = (exp(2.) -exp(1));
        INFO("1D integral               "<<std::setw(6)<<integral);
        INFO("Correct integral is       "<<std::setw(6)<<sol);
        INFO("Relative 1d error is      "<<(integral-sol)/sol);
        CHECK( abs(res.i - 4616944842743393935) < 2);

        double norm = dg::blas2::dot( func1d, w1d, func1d); res.d = norm;
        double solution = (exp(4.) -exp(2))/2.;
        INFO("Square normalized 1D norm "<<std::setw(6)<<norm);
        INFO("Correct square norm is    "<<std::setw(6)<<solution);
        INFO("Relative 1d error is      "<<(norm-solution)/solution);
        CHECK( abs(res.i - 4627337306989890294) < 2);
    }

    SECTION( "2d grid")
    {
        INFO("On Grid 3 x 48 x 48");
        dg::Grid2d g2d( 0.0, 6.2831853071795862, 0.0, 6.2831853071795862, 3,
                48, 48);
        //dg::Grid2d g2d( {0.0, 6.2831853071795862, 3, 48}, {0.0, 6.2831853071795862, 5, 28});
        const dg::DVec func2d = dg::construct<dg::DVec>( dg::evaluate(
                    function<double>, g2d));
        const dg::DVec w2d = dg::construct<dg::DVec>( dg::create::weights( g2d));
        dg::exblas::udouble res;
        double integral2d = dg::blas1::dot( w2d, func2d); res.d = integral2d;
        double sol2d = 0;
        INFO("2D integral               "<<std::setw(6)<<integral2d);
        INFO("Correct integral is       "<<std::setw(6)<<sol2d);
        INFO("2d error is               "<<(integral2d-sol2d));
        CHECK( abs(res.i + 4823286950217646080 < 2));

        double norm2d = dg::blas2::dot( w2d, func2d); res.d = norm2d;
        double solution2d = 80.0489;
        INFO( "Square normalized 2D norm "<<std::setw(6)<<norm2d);
        INFO( "Correct square norm is    "<<std::setw(6)<<solution2d);
        INFO( "Relative 2d error is      "<<(norm2d-solution2d)/solution2d);
        CHECK( abs( res.i - 4635333359953759707) < 2);
    }

    SECTION( "2d grid float")
    {
        INFO("On Grid 3 x 48 x 48 ");
        dg::RealGrid<float,2> gf2d( 0.0, 6.2831853071795862, 0.0,
                6.2831853071795862, 3, 48, 48);
        const dg::fDVec funcf2d = dg::construct<dg::fDVec>( dg::evaluate(
                    function<float>, gf2d));
        const dg::fDVec wf2d = dg::construct<dg::fDVec>( dg::create::weights( gf2d));
        dg::exblas::ufloat resf;
        float integralf2d = dg::blas1::dot( wf2d, funcf2d); resf.f = integralf2d;
        float solf2d = 0;
        INFO("2D integral (float)       "<<std::setw(6)<<integralf2d);
        INFO("Correct integral is       "<<std::setw(6)<<solf2d);
        INFO("2d error (float)          "<<(integralf2d-solf2d));
        // (Remark: in floating precision the function to integrate may already
        // be different on different compilers)
        CHECK(abs( resf.i - 913405508) < 4);
    }

    SECTION( "3d grid")
    {
        unsigned n = 3, Nx = 12, Ny = 28, Nz = 100;
        INFO("On Grid "<<n<<" x "<<Nx<<" x "<<Ny<<" x "<<Nz);

        dg::Grid3d g3d( 1, 2, 3, 4, 5, 6, n, Nx, Ny, Nz,dg::PER,dg::PER,dg::PER);
        //dg::Grid3d g3d( {1, 2, n, Nx,},{ 3, 4, 7, Ny},{ 5, 6, 4, Nx});

        const dg::DVec func3d = dg::construct<dg::DVec>( dg::evaluate(
                    function3d, g3d));
        const dg::DVec w3d = dg::construct<dg::DVec>( dg::create::weights( g3d));

        dg::exblas::udouble res;
        double integral3d = dg::blas1::dot( w3d, func3d); res.d = integral3d;
        double sol3d = (exp(2.)-exp(1))*(exp(4.)-exp(3))*(exp(6.)-exp(5));
        INFO("3D integral               "<<std::setw(6)<<integral3d);
        INFO("Correct integral is       "<<std::setw(6)<<sol3d);
        INFO("Relative 3d error is      "<<(integral3d-sol3d)/sol3d);
        CHECK( abs(res.i - 4675882723962622631) < 2 );

        double solution3d =
            (exp(4.)-exp(2))/2.*(exp(8.)-exp(6.))/2.*(exp(12.)-exp(10))/2.;
        double norm3d = dg::blas2::dot( func3d, w3d, func3d); res.d = norm3d;
        INFO( "Square normalized 3D norm "<<std::setw(6)<<norm3d);
        INFO( "Correct square norm is    "<<std::setw(6)<<solution3d);
        INFO( "Relative 3d error is      "<<(norm3d-solution3d)/solution3d);
        CHECK( abs( res.i - 4746764681002108278) < 2);
    }
}

TEST_CASE( "vdot test")
{

    SECTION( "Size of vector test")
    {
        unsigned n = 3, Nx = 12, Ny = 28, Nz = 100;
        dg::Grid3d g3d( 1, 2, 3, 4, 5, 6, n, Nx, Ny, Nz,dg::PER,dg::PER,dg::PER);
        const dg::DVec v = dg::construct<dg::DVec>( dg::evaluate( function3d,
                    g3d));

        unsigned size = dg::blas1::vdot( []DG_DEVICE(double x){ return 1u;},
                v);
        INFO("Size of vector test       "<<size<<"\t"<<g3d.size());
        CHECK( int( size - g3d.size())  == 0);
    }
}

TEST_CASE( "compiler specific math")
{
    INFO( "TEST result of a sin and exp function to compare compiler specific"
          <<"math libraries:\n");
    SECTION( "sin function")
    {
        dg::DVec x(10, 6.12610567450009658);
        dg::blas1::transform( x, x, [] DG_DEVICE ( double x){ return sin(x);} );
        dg::exblas::udouble res;
        res.d = x[0];
        INFO( "Result of sin:    "<<res.i<<"\t"
                  << "          GCC:    -4628567870976535683 (correct)");
        CHECK( abs( res.i + 4628567870976535683 ) < 2);
    }

    SECTION( "exp function")
    {
        dg::DVec y(10, 5.9126151457310376);
        dg::blas1::transform( y, y,[] DG_DEVICE ( double x){ return exp(x);} );
            dg::exblas::udouble res;
        res.d = y[0];
        INFO( "Result of exp:     "<<res.i<<"\t"
                  << "          GCC:     4645210948416067678 (correct)");
        CHECK( abs( res.i - 4645210948416067678 ) < 2);
    }
}

TEST_CASE("Integral")
{
    using namespace Catch::Matchers;
    dg::Grid1d g1d( 1, 2, 7, 12);
    auto dir = GENERATE( dg::forward, dg::backward);
    //TEST OF INTEGRAL
    dg::HVec integral_num = dg::integrate( cos, g1d, dir);
    dg::HVec integral_ana = dg::evaluate( sin, g1d);
    dg::blas1::plus( integral_ana, dir == dg::forward ? -sin(g1d.x0()) :
            -sin(g1d.x1()));
    dg::blas1::axpby( 1., integral_ana, -1., integral_num);
    double norm = dg::blas2::dot( integral_num, dg::create::weights( g1d),
            integral_num);
    INFO( " Error norm of  1d integral function ("<<dg::direction2str(dir)<< ") "<<norm)
    CHECK_THAT( norm, WithinAbs(  0, 1e-15));
}

TEST_CASE( "Dot throws")
{
    // TEST if dot throws on NaN
    INFO( "TEST if dot throws on Inf or Nan");
    dg::DVec x(10, -10);
    dg::blas1::transform( x,x, dg::LN<double>());
    bool hasnan = dg::blas1::reduce( x, false,
            thrust::logical_or<bool>(), dg::ISNFINITE<double>());
    REQUIRE( hasnan);
    CHECK_THROWS_AS( dg::blas1::dot( x,x), std::exception);
}
TEST_CASE( "MinMod function")
{
    dg::MinMod minmod;
    CHECK( minmod( 3,-5) == 0);
    CHECK( minmod( 2,4,1) == 1);
    CHECK( minmod( 0,1,2) == 0);
    CHECK( minmod( -1,1,2) == 0);
    CHECK( minmod( -5,-3,-2) == -2);
}
TEST_CASE( "Accuracy Dense Matrix")
{
    using namespace Catch::Matchers;
    dg::Grid2d g2d( 0.0, 6.2831853071795862, 0.0, 6.2831853071795862, 3,
                48, 48);
    // massage a scalar product into dg::blas2::symv
    const dg::HVec func_h = dg::evaluate( function<double>, g2d);
    const dg::HVec w_h = dg::create::weights( g2d);
    std::vector<dg::DVec> matrix( func_h.size());
    for( unsigned i=0; i<func_h.size(); i++)
        matrix[i] = dg::DVec( 2, func_h[i]);
    dg::DVec integral_d( 2);
    dg::blas2::symv( 1., dg::asDenseMatrix( dg::asPointers( matrix)), w_h,
            0., integral_d);
    dg::exblas::udouble res;
    res.d = integral_d[0];
    INFO( "2D integral               "<<std::setw(6)<<res.d);
    CHECK( abs( res.i + 4823491540355645440) < 2);
    //We do not expect this to be correct because the Matrix-Vector product is
    //not accurate nor binary reproducible)!
    double sol2d = 0;
    INFO( "Correct integral is       "<<std::setw(6)<<sol2d);
    INFO( "2d error is               "<<(res.d-sol2d));
    CHECK_THAT( fabs(res.d - sol2d), WithinAbs( 0, 1e-13));
}
TEST_CASE( "Complex scalar products")
{
    using namespace Catch::Matchers;
    SECTION( "3d grid")
    {
        unsigned n = 3, Nx = 12, Ny = 28, Nz = 100;
        dg::Grid3d g3d( 1, 2, 3, 4, 5, 6, n, Nx, Ny, Nz,dg::PER,dg::PER,dg::PER);
        const dg::DVec w3d = dg::construct<dg::DVec>( dg::create::weights( g3d));

        const dg::DVec func3d = dg::construct<dg::DVec>( dg::evaluate(
                    function3d, g3d));
        thrust::device_vector<thrust::complex<double>> cc3d( func3d.size());
        dg::blas1::transform( func3d, cc3d, []DG_DEVICE(double x){ return
                thrust::complex<double>{x,x};});
        thrust::complex<double> cintegral = dg::blas1::dot( w3d, cc3d);
        dg::exblas::udouble res;
        res.d =cintegral.real();
        double sol2d = 0;
        INFO( "3D integral (real)        "<<std::setw(6)<<cintegral.real());
        INFO( "Correct integral is       "<<std::setw(6)<<sol2d);
        INFO( "3d error is               "<<(cintegral.real()-sol2d));
        CHECK( abs(res.i - 4675882723962622631) < 2);
        res.d =cintegral.imag();
        INFO( "3D integral (imag)        "<<std::setw(6)<<cintegral.imag());
        CHECK( abs(res.i - 4675882723962622631) < 2);
    }

    SECTION( "1d grid")
    {
        dg::Grid1d g1d( 1, 2, 3, 12);
        const dg::DVec func1d = dg::construct<dg::DVec>( dg::evaluate( exp,
                    g1d));
        thrust::device_vector<thrust::complex<double>> cc1d( func1d.size());
        dg::blas1::transform( func1d, cc1d, []DG_DEVICE(double x){ return
                thrust::complex<double>{x,x};});
        const dg::DVec w1d = dg::construct<dg::DVec>( dg::create::weights( g1d));
        thrust::complex<double> cintegral = dg::blas1::dot( w1d, cc1d);
        dg::exblas::udouble res;
        res.d =cintegral.real();
        INFO( "1D integral (real)        "<<std::setw(6)<<cintegral.real());
        double sol = (exp(2.) -exp(1));
        INFO( "Correct integral is       "<<std::setw(6)<<sol);
        INFO( "Relative 1d error is      "<<(cintegral.real()-sol)/sol);
        CHECK( abs( res.i - 4616944842743393935) < 2);
        res.d =cintegral.imag();
        INFO( "1D integral (imag)        "<<std::setw(6)<<cintegral.imag());
        CHECK( abs( res.i - 4616944842743393935) < 2);
    }
    SECTION( "Vector valued scalar product")
    {
        dg::Grid1d g1d( 1, 2, 7, 12);
        const dg::DVec func1d = dg::construct<dg::DVec>( dg::evaluate( exp,
                    g1d));
        thrust::device_vector<thrust::complex<double>> cc1d( func1d.size());
        dg::blas1::transform( func1d, cc1d, []DG_DEVICE(double x){ return
                thrust::complex<double>{x,x};});
        std::vector<thrust::device_vector<thrust::complex<double>>> vx( 4,
                cc1d);
        const dg::DVec w1d = dg::construct<dg::DVec>( dg::create::weights( g1d));
        std::vector<thrust::device_vector<double>> vw1d( 4, w1d);
        auto cintegral = dg::blas1::dot( vw1d, vx);
        double sol = 4*(exp(2.) -exp(1));
        INFO( "Correct integral is       "<<std::setw(6)<<sol);
        double err = (cintegral.real() - sol)/ sol;
        INFO( "Relative 1d error is      "<<err);
        CHECK_THAT( err, WithinAbs(  0, 1e-15));
    }
}
