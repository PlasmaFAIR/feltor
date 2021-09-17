#include <iostream>
#include <iomanip>

#define DG_BENCHMARK
#include "dg/algorithm.h"
#include "ds.h"
#include "guenther.h"
#include "magnetic_field.h"
#include "testfunctors.h"
#include "ds_generator.h"

const double R_0 = 10;
const double I_0 = 20; //q factor at r=1 is I_0/R_0
const double a  = 1; //small radius

int main( )
{
    std::cout << "# Test the parallel derivative DS in cylindrical coordinates for the guenther flux surfaces. Fieldlines do not cross boundaries.\n";
    std::cout << "# Type n (3), Nx(20), Ny(20), Nz(20)\n";
    unsigned n, Nx, Ny, Nz, mx, my, max_iter = 1e4;
    std::string method = "cubic";
    std::cin >> n>> Nx>>Ny>>Nz;
    std::cout <<"# You typed\n"
              <<"n:  "<<n<<"\n"
              <<"Nx: "<<Nx<<"\n"
              <<"Ny: "<<Ny<<"\n"
              <<"Nz: "<<Nz<<std::endl;
    std::cout << "# Type mx (10) and my (10)\n";
    std::cin >> mx>> my;
    std::cout << "# You typed\n"
              <<"mx: "<<mx<<"\n"
              <<"my: "<<my<<std::endl;
    std::cout << "# Type method (dg, nearest, linear, cubic) \n";
    std::cin >> method;
    method.erase( std::remove( method.begin(), method.end(), '"'), method.end());
    std::cout << "# You typed\n"
              <<"method: "<< method<<std::endl;
    std::cout << "# Create parallel Derivative!\n";
    ////////////////////////////////initialze fields /////////////////////
    const dg::CylindricalGrid3d g3d( R_0 - a, R_0+a, -a, a, 0, 2.*M_PI, n, Nx, Ny, Nz, dg::NEU, dg::NEU, dg::PER);
    const dg::geo::TokamakMagneticField mag = dg::geo::createGuentherField(R_0, I_0);
    dg::geo::DS<dg::aProductGeometry3d, dg::IDMatrix, dg::DMatrix, dg::DVec> ds(
        mag, g3d, dg::NEU, dg::NEU, dg::geo::FullLimiter(),
        1e-8, mx, my, -1, method);

    ///##########################################################///
    const dg::DVec fun = dg::evaluate( dg::geo::TestFunctionPsi2(mag), g3d);
    dg::DVec derivative(fun);
    dg::DVec sol0 = dg::evaluate( dg::geo::DsFunction<dg::geo::TestFunctionPsi2>(mag), g3d);
    dg::DVec sol1 = dg::evaluate( dg::geo::DssFunction<dg::geo::TestFunctionPsi2>(mag), g3d);
    dg::DVec sol2 = dg::evaluate( dg::geo::DsDivFunction<dg::geo::TestFunctionPsi2>(mag), g3d);
    dg::DVec sol3 = dg::evaluate( dg::geo::DsDivDsFunction<dg::geo::TestFunctionPsi2>(mag), g3d);
    dg::DVec sol4 = dg::evaluate( dg::geo::OMDsDivDsFunction<dg::geo::TestFunctionPsi2>(mag), g3d);
    std::vector<std::pair<std::string, std::array<const dg::DVec*,2>>> names{
         {"forward",{&fun,&sol0}},          {"backward",{&fun,&sol0}},
         {"forward2",{&fun,&sol0}},         {"backward2",{&fun,&sol0}},
         {"centered",{&fun,&sol0}},         {"dss",{&fun,&sol1}},
         {"centered_bc_along",{&fun,&sol0}},{"dss_bc_along",{&fun,&sol1}},
         {"divForward",{&fun,&sol2}},       {"divBackward",{&fun,&sol2}},
         {"divCentered",{&fun,&sol2}},      {"directLap",{&fun,&sol3}},
         {"invCenteredLap",{&sol4,&fun}}
    };

    ///##########################################################///
    std::cout << "# TEST Guenther (No Boundary conditions)!\n";
    std::cout <<"Guenther : #rel_Error rel_Volume_integral(should be zero for div and Lap)\n";
    const dg::DVec vol3d = dg::create::volume( g3d);
    for( const auto& tuple :  names)
    {
        std::string name = std::get<0>(tuple);
        const dg::DVec& function = *std::get<1>(tuple)[0];
        const dg::DVec& solution = *std::get<1>(tuple)[1];
        callDS( ds, name, function, derivative, max_iter,1e-8);
        double sol = dg::blas2::dot( vol3d, solution);
        double vol = dg::blas1::dot( vol3d, derivative)/sqrt( dg::blas2::dot( vol3d, function)); // using function in denominator makes entries comparable
        dg::blas1::axpby( 1., solution, -1., derivative);
        double norm = dg::blas2::dot( derivative, vol3d, derivative);
        std::cout <<"    "<<name<<":" <<std::setw(18-name.size())
                  <<" "<<sqrt(norm/sol)<<std::endl
                  <<"    "<<name+"_vol:"<<std::setw(30-name.size())
                  <<" "<<vol<<"\n";
    }
    ///##########################################################///
    std::cout << "# TEST TOTAL VARIATION DIMINISHING\n";
    ds.fieldaligned()(dg::geo::zeroPlus, fun, derivative);
    double mass_before = dg::blas1::dot( vol3d, fun);
    std::cout << "# mass before: "<<mass_before<<"\n";
    double mass_after = dg::blas1::dot( ds.fieldaligned().sqrtGp(), derivative);
    std::cout << "# mass after   "<<mass_after<<"\n";
    std::cout << "# Difference   "<<fabs(mass_before-mass_after)/mass_before<<"\n";
    mass_before = sqrt(dg::blas2::dot( vol3d, fun));
    std::cout << "# l2 norm before: "<<mass_before<<"\n";
    mass_after = sqrt(dg::blas2::dot( ds.fieldaligned().sqrtGp(), derivative));
    std::cout << "# l2 norm after   "<<mass_after<<"\n";
    std::cout << "# Difference   "<<fabs(mass_before-mass_after)/mass_before<<"\n";

    dg::geo::DSPGenerator generator( mag, g3d.x0(), g3d.x1(), g3d.y0(), g3d.y1(), g3d.hz());
    dg::geo::CurvilinearProductGrid3d g3dP( generator, g3d.n(), g3d.Nx(),
        g3d.Ny(), g3d.Nz(), g3d.bcx(), g3d.bcy(), g3d.bcz());
    dg::DVec vol3dP = dg::create::volume( g3dP);

    std::cout << "# Volume on original    grid: "<<dg::blas1::dot( 1., vol3d)<<"\n";
    std::cout << "# Volume on curvilinear grid: "<<dg::blas1::dot( 1., vol3dP)<<"\n";
    std::cout << "# Difference                : "<<dg::blas1::dot( 1., vol3d) - dg::blas1::dot( 1., vol3dP)<<"\n";
    dg::Elliptic<dg::aProductGeometry3d, dg::DMatrix, dg::DVec> elliptic(g3d,
        dg::normed);
    dg::DVec variation(fun);
    elliptic.variation( fun, variation);
    dg::blas1::transform( variation, variation, []DG_DEVICE( double var){ return var/sqrt(var);});
    double var_before = dg::blas1::dot( vol3d, variation);
    std::cout << "# variation before: "<<var_before<<"\n";
    elliptic.construct(g3dP, dg::normed);
    elliptic.variation( derivative, variation);
    dg::blas1::transform( variation, variation, []DG_DEVICE( double var){ return var/sqrt(var);});
    double var_after = dg::blas1::dot( vol3d, variation);
    std::cout << "# variation after   "<<var_after<<"\n";
    std::cout << "# Difference (Af-Be)"<<var_after-var_before<<"\n";
    ///##########################################################///
    std::cout << "# TEST STAGGERED GRID DERIVATIVE\n";
    dg::DVec zMinus(fun), eMinus(fun), zPlus(fun), ePlus(fun);
    dg::DVec funST(fun);
    dg::geo::Fieldaligned<dg::aProductGeometry3d,dg::IDMatrix,dg::DVec>  dsFAST(
            mag, g3d, dg::NEU, dg::NEU, dg::geo::NoLimiter(), 1e-8, mx, my,
            g3d.hz()/2., method);
    dsFAST( dg::geo::zeroMinus, fun, zMinus);
    dsFAST( dg::geo::einsPlus,  fun, ePlus);
    dg::geo::ds_slope( dsFAST, 1., zMinus, ePlus, 0., funST);
    dsFAST( dg::geo::zeroPlus, funST, zPlus);
    dsFAST( dg::geo::einsMinus, funST, eMinus);
    dg::geo::ds_average( dsFAST, 1., eMinus, zPlus, 0., derivative);

    double sol = dg::blas2::dot( vol3d, sol0);
    double vol = dg::blas1::dot( vol3d, derivative)/sqrt( dg::blas2::dot( vol3d, fun));
    dg::blas1::axpby( 1., sol0, -1., derivative);
    double norm = dg::blas2::dot( derivative, vol3d, derivative);
    std::string name  = "centeredST";
    std::cout <<"    "<<name<<":" <<std::setw(18-name.size())
              <<" "<<sqrt(norm/sol)<<"\n"
              <<"    "<<name+"_vol:"<<std::setw(30-name.size())
              <<" "<<vol<<"\n";

    ds.fieldaligned()(dg::geo::einsPlus, fun, ePlus);
    ds.fieldaligned()(dg::geo::einsMinus, fun, eMinus);
    dg::blas1::pointwiseDot ( 1./2./dsFAST.deltaPhi(), dsFAST.bphiM(),
            fun, -1./2./dsFAST.deltaPhi(), dsFAST.bphiM(),
            eMinus, 0., eMinus);
    dg::blas1::pointwiseDot( 1./2./dsFAST.deltaPhi(), ePlus,
            dsFAST.bphiP(), -1./2./dsFAST.deltaPhi(), fun,
            dsFAST.bphiP(), 0., ePlus);
    dg::geo::ds_divCentered( dsFAST, 1., eMinus, ePlus, 0., derivative);
    sol = dg::blas2::dot( vol3d, sol3);
    vol = dg::blas1::dot( vol3d, derivative)/sqrt( dg::blas2::dot( vol3d, fun));
    dg::blas1::axpby( 1., sol3, -1., derivative);
    norm = dg::blas2::dot( derivative, vol3d, derivative);
    name  = "directLapST"; // works as well as directLap
    std::cout <<"    "<<name<<":" <<std::setw(18-name.size())
              <<" "<<sqrt(norm/sol)<<"\n"
              <<"    "<<name+"_vol:"<<std::setw(30-name.size())
              <<" "<<vol<<"\n";
    ///##########################################################///
    std::cout << "# TEST VOLUME FORMS\n";
    double volume = dg::blas1::dot( 1., dsFAST.sqrtG());
    double volumeM = dg::blas1::dot( 1., dsFAST.sqrtGm());
    double volumeP = dg::blas1::dot( 1., dsFAST.sqrtGp());
    std::cout << "volume_error:\n";
    std::cout <<"    minus:"<<std::setw(13)<<" "<<fabs(volumeM-volume)/volume<<"\n";
    std::cout <<"    plus:" <<std::setw(14)<<" "<<fabs(volumeP-volume)/volume<<"\n";


    dg::DVec f(g3d.size(), 1.), temp1(f), temp2(f), temp3(f);
    dsFAST(dg::geo::einsPlus, f, temp1);

    dg::blas1::pointwiseDot( dsFAST.sqrtG(), temp1, temp3);
    dsFAST(dg::geo::einsPlusT, temp3, temp2);
    dg::blas1::pointwiseDivide( temp2, dsFAST.sqrtGm(), temp2);
    dg::blas1::axpby( 1., temp2, -1., 1., temp2);
    dsFAST(dg::geo::einsPlus, temp2, temp3);

    double error = dg::blas2::dot( temp3, vol, temp3);
    //norm = dg::blas2::dot( 1., vol, 1.);
    norm = dg::blas2::dot( temp1, vol, temp1);
    std::cout <<"    Inv:"<<std::setw(15)<<" "<<sqrt(error/norm)<<"\n";

    return 0;
}
