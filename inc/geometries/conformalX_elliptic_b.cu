#include <iostream>

#include "dg/algorithm.h"

#include "solovev.h"
#include "taylor.h"
//#include "ribeiroX.h"
#include "curvilinearX.h"
#include "separatrix_orthogonal.h"
#include "testfunctors.h"


void compute_error_elliptic( const dg::geo::TokamakMagneticField& c, const dg::geo::CurvilinearGridX2d& g2d, dg::DVec& x, double psi_0, double psi_1, double eps)
{
    dg::Elliptic<dg::geo::CurvilinearGridX2d, dg::Composite<dg::DMatrix>, dg::DVec> pol( g2d, dg::not_normed, dg::forward);
    ////////////////////////blob solution////////////////////////////////////////
    //const dg::DVec b        = dg::pullback( dg::geo::EllipticBlobDirNeuM(c,psi_0, psi_1, 480, -300, 70.,1.), g2d);
    //const dg::DVec chi      = dg::pullback( dg::ONE(), g2d);
    //const dg::DVec solution = dg::pullback( dg::geo::FuncDirNeu(c, psi_0, psi_1, 480, -300, 70., 1. ), g2d);
    //////////////////////////blob solution on X-point/////////////////////////////
    const dg::DVec b        = dg::pullback( dg::geo::EllipticBlobDirNeuM(c,psi_0, psi_1, 420, -470, 50.,1.), g2d);
    const dg::DVec chi      = dg::pullback( dg::ONE(), g2d);
    const dg::DVec solution = dg::pullback( dg::geo::FuncDirNeu(c, psi_0, psi_1, 420, -470, 50., 1. ), g2d);
    ////////////////////////////laplace psi solution/////////////////////////////
    //const dg::DVec b =        dg::pullback( c.laplacePsip);
    //const dg::DVec chi =      dg::evaluate( dg::one, g2d);
    //const dg::DVec solution =     dg::pullback( c.psip, g2d);
    /////////////////////////////Dir/////FIELALIGNED SIN///////////////////
    //const dg::DVec b = dg::pullback( dg::geo::EllipticXDirNeuM(c, psi_0, psi_1), g2d);
    //dg::DVec chi     = dg::pullback( dg::geo::Bmodule(c), g2d);
    //dg::blas1::plus( chi, 1e4);
    //const dg::DVec chi =  dg::pullback( dg::ONE(), g2d);
    //const dg::DVec solution = dg::pullback( dg::geo::FuncXDirNeu(c, psi_0, psi_1 ), g2d);
    ////////////////////////////////////////////////////////////////////////////

    const dg::DVec vol2d = dg::create::volume( g2d);
    const dg::DVec inv_vol2d = dg::create::inv_volume( g2d);
    const dg::DVec v2d = dg::create::inv_weights( g2d);
    pol.set_chi( chi);
    //compute error
    dg::DVec error( solution);
    std::cout << eps<<"\t";
    dg::Timer t;
    t.tic();
    dg::Invert<dg::DVec > invert( x, g2d.size(), eps);
    //unsigned number = invert(pol, x,b, vol2d, inv_vol2d );
    unsigned number = invert(pol, x,b, vol2d, v2d, v2d ); //inv weights are better preconditioners
    std::cout <<number<<"\t";
    t.toc();
    dg::blas1::axpby( 1.,x,-1., solution, error);
    double err = dg::blas2::dot( vol2d, error);
    const double norm = dg::blas2::dot( vol2d, solution);
    std::cout << sqrt( err/norm) << "\t";
    std::cout<<t.diff()/(double)number<<"s"<<std::endl;
}
template<class Geometry>
void compute_cellsize( const Geometry& g2d)
{
    ///////////////////////////////////metric//////////////////////
    dg::SparseTensor<dg::DVec> metric = g2d.metric();
    dg::DVec gyy = metric.value(1,1), gxx = metric.value(0,0), vol = dg::tensor::volume(metric).value(); 
    dg::blas1::transform( gxx, gxx, dg::SQRT<double>());
    dg::blas1::transform( gyy, gyy, dg::SQRT<double>());
    dg::blas1::pointwiseDot( gxx, vol, gxx);
    dg::blas1::pointwiseDot( gyy, vol, gyy);
    dg::blas1::scal( gxx, g2d.hx());
    dg::blas1::scal( gyy, g2d.hy());
    double hxX = dg::interpolate( 0, 0, gxx, g2d);
    double hyX = dg::interpolate( 0, 0, gyy, g2d);
    std::cout << *thrust::max_element( gxx.begin(), gxx.end()) << "\t";
    std::cout << *thrust::max_element( gyy.begin(), gyy.end()) << "\t";
    std::cout << hxX << "\t";
    std::cout << hyX << "\t";
}

int main(int argc, char**argv)
{
    std::cout << "Type n, Nx (fx = 1./4.), Ny (fy = 1./22.)\n";
    unsigned n, Nx, Ny;
    std::cin >> n>> Nx>>Ny;   
    std::cout << "Type psi_0 (-15)! \n";
    double psi_0, psi_1;
    std::cin >> psi_0;
    unsigned nIter = 3;
    std::cout << "type # iterations! (6)\n";
    std::cin >> nIter;
    Json::Value js;
    if( argc==1)
    {
        std::ifstream is( "geometry_params_Xpoint.js");
        is >> js;
    }
    else
    {
        std::ifstream is(argv[1]);
        is >> js;
    }
    dg::geo::solovev::Parameters gp(js);
    dg::geo::TokamakMagneticField c = dg::geo::createSolovevField(gp);
    ////////////////construct Generator////////////////////////////////////
    double R0 = gp.R_0, Z0 = 0;
    double R_X = gp.R_0-1.1*gp.triangularity*gp.a;
    double Z_X = -1.1*gp.elongation*gp.a;
    /////////////no monitor
    //dg::geo::BinarySymmTensorLvl1 monitor_chi;
    ////////////const monitor
    dg::geo::BinarySymmTensorLvl1 monitor_chi = make_Xconst_monitor( c.get_psip(), R_X, Z_X) ;
    /////////////monitor bumped around X-point
    //double radius;
    //std::cout << "Type radius\n";
    //std::cin >> radius;
    //dg::geo::BinarySymmTensorLvl1 monitor_chi = make_Xbump_monitor( c.get_psip(), R_X, Z_X, radius, radius) ;
    /////////////
    double fx = 0.25;
    psi_1 = -fx/(1.-fx)*psi_0;
    std::cout << "psi_0 = "<<psi_0<<" psi_1 = "<<psi_1<<"\n";
    dg::geo::SeparatrixOrthogonal generator(c.get_psip(), monitor_chi, psi_0, R_X,Z_X, R0, Z0,0);
    std::cout << "eps \t# iterations error \thxX hyX \thx_max hy_max \ttime/iteration \n";
    std::cout << "Computing on "<<n<<" x "<<Nx<<" x "<<Ny<<"\n";
    const double eps = 1e-11;
    dg::geo::CurvilinearGridX2d g2d( generator, 0.25, 1./22., n, Nx, Ny, dg::DIR, dg::DIR);
    dg::DVec x = dg::evaluate( dg::zero, g2d);
    compute_error_elliptic(c, g2d, x, psi_0, psi_1, eps);
    compute_cellsize(g2d);
    std::cout <<std::endl;
    for( unsigned i=1; i<nIter; i++)
    {
        Nx*=2; Ny*=2;
        dg::MultiMatrix<dg::DMatrix, dg::DVec >  inter = dg::create::fast_interpolation(g2d.grid(), 2, 2);
        dg::geo::CurvilinearGridX2d g2d_new( generator, 0.25, 1./22., n, Nx, Ny, dg::DIR, dg::DIR);
        std::cout << "Computing on "<<n<<" x "<<Nx<<" x "<<Ny<<"\n";
        dg::DVec x_new = dg::evaluate( dg::zero, g2d_new);
        dg::blas2::symv( inter, x, x_new);
        compute_error_elliptic(c, g2d_new, x_new, psi_0, psi_1, eps);
        compute_cellsize(g2d_new);
        std::cout <<std::endl;
        g2d = g2d_new; x = x_new;
    }


    return 0;
}
