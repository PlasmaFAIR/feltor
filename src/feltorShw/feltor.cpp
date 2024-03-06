#include <iostream>
#include <iomanip>
#include <vector>
#include <sstream>
#include <cmath>

#ifdef WITH_GLFW
#include "draw/host_window.h"
#endif
#ifdef WITH_MPI
#include <mpi.h> //activate mpi
#endif

#include "dg/file/file.h"

#include "feltor.h"
#include "parameters.h"


struct Variables
{
    eule::Explicit<dg::x::CartesianGrid2d, dg::x::DMatrix, dg::x::DVec>& feltor;
    eule::Implicit<dg::x::CartesianGrid2d, dg::x::DMatrix, dg::x::DVec>& rolkar;
    const dg::x::Grid2d& grid;
    const eule::Parameters& p;
    const std::vector<dg::x::DVec>& y0;
    dg::x::DMatrix dy;
    const double& time;
};

int main( int argc, char* argv[])
{
#ifdef WITH_MPI
    dg::mpi_init( argc, argv);
    MPI_Comm comm;
    dg::mpi_init2d( dg::DIR, dg::PER, comm, std::cin, true);
    int rank;
    MPI_Comm_rank( MPI_COMM_WORLD, &rank);
#endif //WITH_MPI
    ////////////////////////Parameter initialisation//////////////////////////
    std::string input;
    if( argc == 1)
        input = "input.json";
    else
        input = argv[1];
    dg::file::WrappedJsonValue js( dg::file::error::is_throw);
    eule::Parameters p;
    try{
        js = dg::file::file2Json( input);
        p = { js};
    } catch( std::exception& e) {
        DG_RANK0 std::cerr << "ERROR in input file "<<input<<std::endl;
        DG_RANK0 std::cerr << e.what()<<std::endl;
        dg::abort_program();
    }
    DG_RANK0 std::cout << js.toStyledString() << std::endl;
    DG_RANK0 p.display(std::cout);
    //////////////////////////////////////////////////////////////////////////

    //Make grid
    dg::x::Grid2d grid(     0., p.lx, 0.,p.ly, p.n,     p.Nx,     p.Ny,     p.bc_x, p.bc_y
        #ifdef WITH_MPI
        , comm
        #endif //WITH_MPI
        );
    dg::x::Grid2d grid_out( 0., p.lx, 0.,p.ly, p.n_out, p.Nx_out, p.Ny_out, p.bc_x, p.bc_y
        #ifdef WITH_MPI
        , comm
        #endif //WITH_MPI
        );
    //create RHS
    DG_RANK0 std::cout << "Constructing Explicit...\n";
    eule::Explicit<dg::x::CartesianGrid2d, dg::x::DMatrix, dg::x::DVec > feltor( grid, p); //initialize before rolkar!
    DG_RANK0 std::cout << "Constructing Implicit...\n";
    eule::Implicit<dg::x::CartesianGrid2d, dg::x::DMatrix, dg::x::DVec > rolkar( grid, p);
    DG_RANK0 std::cout << "Done!\n";

    /////////////////////The initial field///////////////////////////////////////////

    dg::ExpProfX prof(p.nprofileamp, p.bgprofamp,p.invkappa);
    std::vector<dg::x::DVec> y0(2, dg::evaluate( prof, grid)), y1(y0);
    double time =0;

    if (argc==4) {
        dg::file::NC_Error_Handle errIN;
        int ncidIN;
        errIN = nc_open( argv[3], NC_NOWRITE, &ncidIN);
        dg::file::Reader<dg::x::Grid0d> reader0d( ncidIN, {}, {"time"});
        unsigned size_time = reader0d.size();
        reader0d.get( "time", time, size_time-1);
        DG_RANK0 std::cout << " Current time = "<< time <<  std::endl;

        dg::file::WrappedJsonValue atts( dg::file::nc_attrs2json( ncidIN, NC_GLOBAL));
        dg::file::WrappedJsonValue jsIN = dg::file::string2Json(
            atts["inputfile"].asString(), dg::file::comments::are_forbidden);
        const eule::Parameters pIN(  jsIN);
        DG_RANK0 std::cout << "[input.nc] file parameters" << std::endl;
        DG_RANK0 pIN.display( std::cout);

        dg::x::Grid2d grid_IN( 0., pIN.lx, 0., pIN.ly, pIN.n_out, pIN.Nx_out, pIN.Ny_out, pIN.bc_x, pIN.bc_y
            #ifdef WITH_MPI
            , grid.communicator()
            #endif //WITH_MPI
        );

        dg::file::Reader<dg::x::Grid2d> restart( ncidIN, grid, {"time", "y", "x"});
        dg::x::HVec transferIN( dg::evaluate(dg::zero, grid_IN));
        dg::x::DVec transferIND( dg::evaluate(dg::zero, grid_IN));
        dg::x::IDMatrix interpolateIN = dg::create::interpolation( grid,grid_IN);
        std::string namesIN[2] = {"electrons", "ions"};
        for( unsigned i=0; i<2; i++)
        {
            restart.get( namesIN[i], transferIN, size_time-1);
            dg::assign(transferIN,transferIND);
            dg::blas2::gemv( interpolateIN, transferIND, y0[i]);
        }
        errIN = nc_close(ncidIN);
    }
    else
    {
        if (p.initmode == 0) {
          dg::Gaussian init0( p.posX*p.lx, p.posY*p.ly, p.sigma, p.sigma, p.amp);
          y1[1] = dg::evaluate( init0, grid);
        }
        if (p.initmode == 1) {
          dg::SinXCosY init0(p.amp,0.,2.*M_PI/p.lx,p.sigma*2.*M_PI/p.ly);
          y1[1] = dg::evaluate( init0, grid);
        }
        if (p.initmode == 2) {
          dg::BathRZ init0(16,16,0.,0., 30.,5.,p.amp);
          y1[1] = dg::evaluate( init0, grid);
          dg::x::DVec  dampr = dg::evaluate(dg::TanhProfX(p.lx*0.95,p.sourcew,-1.0,0.0,1.0),grid);
          dg::x::DVec  dampl = dg::evaluate(dg::TanhProfX(p.lx*0.05,p.sourcew,1.0,0.0,1.0),grid);
          dg::blas1::pointwiseDot(y1[1],dampr,y1[1]);
          dg::blas1::pointwiseDot(y1[1],dampl,y1[1]);
        }
        if (p.modelmode == 0 || p.modelmode == 1)
        {
            dg::blas1::pointwiseDot(y1[1], y0[1],y1[1]); //<n>*ntilde
            dg::blas1::axpby( 1., y1[1], 1., y0[1]); //initialize ni = <n> + <n>*ntilde
            dg::blas1::transform(y0[1], y0[1], dg::PLUS<>(-(p.bgprofamp + p.nprofileamp))); //initialize ni-1
            DG_RANK0 std::cout << "intiialize ne" << std::endl;
            feltor.initializene( y0[1], y0[0]);
            DG_RANK0 std::cout << "Done!\n";
        }
        if (p.modelmode == 2) {
            DG_RANK0 std::cout << "intiialize ne" << std::endl;
            dg::blas1::axpby(1.0,y1[1],0.,y0[1],y0[1]);
            feltor.initializene( y1[1], y0[0]);
            DG_RANK0 std::cout << "Done!\n";
        }
        if (p.modelmode == 3) {
            dg::blas1::pointwiseDot( y0[1],y1[1],y0[1]); //<n>*Ntilde
            dg::blas1::axpby( 1., y0[1], 1.,y1[0], y0[1]); //initialize Ni = <n> + <n>*Ntilde
            dg::blas1::transform(y0[1], y0[1], dg::PLUS<>(-(p.bgprofamp + p.nprofileamp))); //initialize Ni-1

            DG_RANK0 std::cout << "intiialize ne" << std::endl;
            feltor.initializene( y0[1], y0[0]); //n_e-1

            dg::blas1::transform( y1[1], y0[1], dg::PLUS<>(+1.0)); // (1+Nitilde)
            dg::blas1::transform( y0[1], y0[1], dg::LN<double>()); //ln (1+Nitilde)

            dg::blas1::transform(y0[0], y0[0], dg::PLUS<>((p.bgprofamp + p.nprofileamp))); //ne
            dg::blas1::pointwiseDivide(y0[0], y1[0],y0[0]); // 1+ netilde
            dg::blas1::transform( y0[0], y0[0], dg::LN<double>()); //ln (1+netilde)

            DG_RANK0 std::cout << "Done!\n";
        }
    }



    dg::DefaultSolver< std::vector<dg::x::DVec> > solver( rolkar, y0,
            y0[0].size(), p.eps_time);
    dg::ImExMultistep< std::vector<dg::x::DVec> > karniadakis( "ImEx-BDF-3-3", y0);
    DG_RANK0 std::cout << "initialize karniadakis" << std::endl;
    karniadakis.init( std::tie(feltor, rolkar, solver), 0., y0, p.dt);
    DG_RANK0 std::cout << "Done!\n";

    const double mass0 = feltor.mass(), mass_blob0 = mass0 - grid.lx()*grid.ly();
    double E0 = feltor.energy(), energy0 = E0, E1 = 0., diff = 0.;

    DG_RANK0 std::cout << "Begin computation \n";
    DG_RANK0 std::cout << std::scientific << std::setprecision( 2);

    std::string output = "netcdf";
    dg::Timer t;
    t.tic();
#ifdef WITH_GLFW
    output = "glfw";
    if( "glfw" == output)
    {
        dg::DVec dvisual( grid.size(), 0.);
        dg::DVec dvisual2( grid.size(), 0.);
        dg::HVec hvisual( grid.size(), 0.), visual(hvisual),avisual(hvisual);
        dg::IHMatrix equi = dg::create::backscatter( grid);
        draw::ColorMapRedBlueExtMinMax colors(-1.0, 1.0);
        /////////glfw initialisation ////////////////////////////////////////////
        std::stringstream title;
        dg::file::WrappedJsonValue js = dg::file::file2Json( "window_params.json");
        GLFWwindow* w = draw::glfwInitAndCreateWindow( js["cols"].asUInt()*js["width"].asUInt()*p.lx/p.ly, js["rows"].asUInt()*js["height"].asUInt(), "");
        draw::RenderHostData render(js["rows"].asUInt(), js["cols"].asUInt());
        unsigned step=0;
        while ( !glfwWindowShouldClose( w ))
        {

            dg::assign(y0[0], hvisual);
    //         if 
    //         dg::blas1::axpby(1.0,hvisual,
            dg::blas2::gemv( equi, hvisual, visual);
            colors.scalemax() = (double)thrust::reduce( visual.begin(), visual.end(), (double)-1e14, thrust::maximum<double>() );
    //         colors.scalemin() = -colors.scalemax();        
            //colors.scalemin() = 1.0;
            colors.scalemin() =  (double)thrust::reduce( visual.begin(), visual.end(), colors.scalemax()  ,thrust::minimum<double>() );

            title << std::setprecision(2) << std::scientific;
            //title <<"ne / "<<(double)thrust::reduce( visual.begin(), visual.end(), colors.scalemax()  ,thrust::minimum<double>() )<<"  " << colors.scalemax()<<"\t";
            title <<"ne-1 / " << colors.scalemax()<< " "<< colors.scalemin()<<"\t";

            render.renderQuad( visual, grid.n()*grid.Nx(), grid.n()*grid.Ny(), colors);

            //draw ions
            //thrust::transform( y1[1].begin(), y1[1].end(), dvisual.begin(), dg::PLUS<double>(-0.));//ne-1
            dg::assign(y0[1], hvisual);
            dg::blas2::gemv( equi, hvisual, visual);
            colors.scalemax() = (double)thrust::reduce( visual.begin(), visual.end(),  (double)-1e14, thrust::maximum<double>() );
            //colors.scalemin() = 1.0;        
    //         colors.scalemin() = -colors.scalemax();        
            colors.scalemin() =  (double)thrust::reduce( visual.begin(), visual.end(), colors.scalemax()  ,thrust::minimum<double>() );

            title << std::setprecision(2) << std::scientific;
            //title <<"ni / "<<(double)thrust::reduce( visual.begin(), visual.end(), colors.scalemax()  ,thrust::minimum<double>() )<<"  " << colors.scalemax()<<"\t";
            title <<"ni-1 / " << colors.scalemin()<<"\t";

            render.renderQuad(visual, grid.n()*grid.Nx(), grid.n()*grid.Ny(), colors);

            
            //draw potential
            //transform to Vor
    //        dvisual=feltor.potential()[0];
    //        dg::blas2::gemv( rolkar.laplacianM(), dvisual, y1[1]);
    //        hvisual = y1[1];
            dg::assign(feltor.potential()[0], hvisual);
            dg::blas2::gemv( equi, hvisual, visual);
            colors.scalemax() = (double)thrust::reduce( visual.begin(), visual.end(),  (double)-1e14, thrust::maximum<double>() );

            colors.scalemin() =  (double)thrust::reduce( visual.begin(), visual.end(), colors.scalemax() ,thrust::minimum<double>() );

    //         //colors.scalemin() = 1.0;        
    //          colors.scalemin() = -colors.scalemax();        
    //          colors.scalemin() = -colors.scalemax();        
            //colors.scalemin() =  (double)thrust::reduce( visual.begin(), visual.end(), colors.scalemax()  ,thrust::minimum<double>() );
            title <<"Potential / "<< colors.scalemax() << " " << colors.scalemin()<<"\t";

            render.renderQuad( visual, grid.n()*grid.Nx(), grid.n()*grid.Ny(), colors);
            //draw potential
            //transform to Vor
            dvisual=feltor.potential()[0];
            dg::blas2::gemv( rolkar.laplacianM(), dvisual, y1[1]);
            dg::assign(y1[1], hvisual);
             //hvisual = feltor.potential()[0];
            dg::blas2::gemv( equi, hvisual, visual);
            colors.scalemax() = (double)thrust::reduce( visual.begin(), visual.end(),  (double)-1e14, thrust::maximum<double>() );
            //colors.scalemin() = 1.0;        
    //          colors.scalemin() = -colors.scalemax();        
            colors.scalemin() =  (double)thrust::reduce( visual.begin(), visual.end(), colors.scalemax()  ,thrust::minimum<double>() );
            title <<"Omega / "<< colors.scalemax()<< " "<< colors.scalemin()<<"\t";

            render.renderQuad( visual, grid.n()*grid.Nx(), grid.n()*grid.Ny(), colors);


         
               
            title << std::fixed; 
            title << " &&   time = "<<time;
            glfwSetWindowTitle(w,title.str().c_str());
            title.str("");
            glfwPollEvents();
            glfwSwapBuffers( w);

            //step 
            t.tic();
            for( unsigned i=0; i<p.itstp; i++)
            {
                try{ karniadakis.step( std::tie(feltor, rolkar, solver), time, y0);}
                catch( dg::Fail& fail) { 
                    std::cerr << "CG failed to converge to "<<fail.epsilon()<<"\n";
                    std::cerr << "Does Simulation respect CFL condition?\n";
                    glfwSetWindowShouldClose( w, GL_TRUE);
                    break;
                }
                step++;
                std::cout << "(m_tot-m_0)/m_0: "<< (feltor.mass()-mass0)/mass_blob0<<"\t";
                E1 = feltor.energy();
                diff = (E1 - E0)/p.dt; //
                double diss = feltor.energy_diffusion( );
    //             double coupling = feltor.coupling();
                std::cout << "(E_tot-E_0)/E_0: "<< (E1-energy0)/energy0<<"\t";
                std::cout << 
                             " Charge= " << feltor.charge() <<
                             " Accuracy: "<< 2.*fabs((diff-diss)/(diff+diss))<<
                             " d E/dt = " << diff <<
                             " Lambda =" << diss <<  std::endl;
                E0 = E1;
            }
            dg::blas1::transform( y0[0], dvisual, dg::PLUS<>(+(p.bgprofamp + p.nprofileamp))); //npe = N+1
            dvisual2 = feltor.potential()[0];
    //         p.profiles
            t.toc();
            std::cout << "\n\t Step "<<step;
            std::cout << "\n\t Average time for one step: "<<t.diff()/(double)p.itstp<<"s\n\n";
        }
        glfwTerminate();
    }
#endif //WITH_GLFW
    if( "netcdf" == output)
    {
        if( argc != 3 && argc != 4)
        {
            DG_RANK0 std::cerr << "ERROR: Wrong number of arguments for netcdf output!\nUsage: "
                    << argv[0]<<" [input.json] [output.nc]\n OR \n"
                    << argv[0]<<" [input.json] [output.nc] [initial.nc] "<<std::endl;
            dg::abort_program();
        }
        /////////////////////////////set up netcdf/////////////////////////////////////
        dg::file::NC_Error_Handle err;
        std::string outputfile = argv[2];
        int ncid=-1;
        try{
            DG_RANK0 err = nc_create(outputfile.c_str(), NC_NETCDF4|NC_CLOBBER, &ncid);
        }catch( std::exception& e)
        {
            DG_RANK0 std::cerr << "ERROR creating file "<<argv[1]<<std::endl;
            DG_RANK0 std::cerr << e.what() << std::endl;
            dg::abort_program();
        }
        dg::file::JsonType att;
        att["title"] = "Output file of feltor/src/feltorShw/feltor.cpp";
        att["Conventions"] = "CF-1.8";
        ///Get local time and begin file history
        auto ttt = std::time(nullptr);

        std::ostringstream oss;
        ///time string  + program-name + args
        oss << std::put_time(std::localtime(&ttt), "%F %T %Z");
        for( int i=0; i<argc; i++) oss << " "<<argv[i];
        att["history"] = oss.str();
        att["comment"] = "Find more info in feltor/src/feltorShw/feltorShw.tex";
        att["source"] = "FELTOR";
        att["references"] = "https://github.com/feltor-dev/feltor";
        // Here we put the inputfile as a string without comments so that it can be read later by another parser
        att["inputfile"] = js.toStyledString();
        DG_RANK0 dg::file::json2nc_attrs( att, ncid, NC_GLOBAL);

        dg::x::DMatrix dy = dg::create::dy( grid, p.bc_y, dg::centered);
        Variables var = { feltor, rolkar, grid, p, y0, dy, time};
        std::vector<dg::file::Record<void( dg::x::DVec& result, Variables&)>> records = {
            {"electrons", "",
                []( dg::x::DVec& result, Variables& v) {
                    dg::blas1::copy( v.y0[0], result);
                }
            },
            {"ions", "",
                []( dg::x::DVec& result, Variables& v) {
                    dg::blas1::copy( v.y0[1], result);
                }
            },
            {"potential", "",
                []( dg::x::DVec& result, Variables& v) {
                    dg::blas1::copy( v.feltor.potential()[0], result);
                }
            },
            {"vor", "",
                []( dg::x::DVec& result, Variables& v) {
                    dg::blas2::gemv(v.rolkar.laplacianM(), v.feltor.potential()[0], result);
                }
            }
        };
        dg::x::IHMatrix interpolate = dg::create::interpolation( grid_out, grid);
        dg::file::WriteRecordsList<dg::x::Grid2d> writer(ncid, grid_out, {"time", "y", "x"});
        dg::file::Writer<dg::x::Grid0d> writ0d( ncid, {}, {"time"});
        dg::x::DVec result = dg::evaluate( dg::zero, grid);
        writ0d.stack("time", time);
        writer.host_transform_write( interpolate, records, result, var);

        std::vector<dg::file::Record<double(Variables&)>> records0d = {
            {"energy_time", "",
                []( Variables& v) {
                    return v.time;
                }
            },
            {"energy", "",
                []( Variables& v) {
                    return v.feltor.energy();
                }
            },
            {"mass", "",
                []( Variables& v) {
                    return v.feltor.mass();
                }
            },
            {"diffusion", "",
                []( Variables& v) {
                    return v.feltor.mass_diffusion();
                }
            },
            {"Se", "",
                []( Variables& v) {
                    return v.feltor.energy_vector()[0];
                }
            },
            {"Si", "",
                []( Variables& v) {
                    return v.feltor.energy_vector()[1];
                }
            },
            {"Uperp", "",
                []( Variables& v) {
                    return v.feltor.energy_vector()[2];
                }
            },
            {"dissipation", "",
                []( Variables& v) {
                    return v.feltor.energy_diffusion();
                }
            },
            {"G_nex", "",
                []( Variables& v) {
                    return v.feltor.radial_transport();
                }
            },
            {"Coupling", "",
                []( Variables& v) {
                    return v.feltor.coupling();
                }
            }
        };
        dg::file::WriteRecordsList<dg::x::Grid0d> writ_records0d(ncid, {}, {"energy_time"});
        writ_records0d.write( records0d, var);

        dg::HVec xprobecoords(7,1.);
        for (unsigned i=0;i<7; i++) {
            xprobecoords[i] = p.lx/8.*(1+i) ;
        }
        const dg::HVec yprobecoords(7,p.ly/2.);
        std::vector<dg::HVec> coords = {xprobecoords, yprobecoords};
        dg::file::ProbesParams probes_params = {
            coords, {"xprobe", "yprobe"}, "none", true
        };
        dg::file::Probes<dg::x::Grid2d> probes( ncid, grid, probes_params);
        std::vector<dg::file::Record<void( dg::x::DVec&, Variables&)>> probe_list =
        {
            {"electrons", "",
                [](dg::x::DVec& result, Variables& var){
                    dg::blas1::copy( var.y0[0], result);
                }
            },
            {"phi", "",
                [](dg::x::DVec& result, Variables& var){
                    dg::blas1::copy( var.feltor.potential()[0], result);
                }
            },
            {"phi_y", "Derivative in y direction",
                [](dg::x::DVec& result, Variables& var){
                    dg::blas2::gemv( var.dy, var.feltor.potential()[0], result);
                },
            },
            {"gamma_x", "radial particle flux",
                [](dg::x::DVec& result, Variables& var){
                    dg::blas2::gemv( var.dy, var.feltor.potential()[0], result);
                    dg::blas1::pointwiseDot( -1., result, var.y0[0], 0., result);
                }
            }
        };
        probes.write( time, probe_list, var);
        DG_RANK0 std::cout << "First write successful!\n";
        for( unsigned i=1; i<=p.maxout; i++)
        {
            for( unsigned j=0; j<p.itstp; j++)
            {
                try{ karniadakis.step( std::tie(feltor, rolkar, solver), time, y0);}
                catch( dg::Fail& fail) {
                    std::cerr << "CG failed to converge to "<<fail.epsilon()<<"\n";
                    std::cerr << "Does Simulation respect CFL condition?\n";
                    err = nc_close(ncid);
                    return -1;
                }
                probes.write( time, probe_list, var);
                writ_records0d.write( records0d, var);
            }
            writer.host_transform_write( interpolate, records, result, var);
            writ0d.stack("time", time);
        }
    }
    ////////////////////////////////////////////////////////////////////
    t.toc();
    unsigned hour = (unsigned)floor(t.diff()/3600);
    unsigned minute = (unsigned)floor( (t.diff() - hour*3600)/60);
    double second = t.diff() - hour*3600 - minute*60;
    DG_RANK0 std::cout << std::fixed << std::setprecision(2) <<std::setfill('0');
    DG_RANK0 std::cout <<"Computation Time \t"<<hour<<":"<<std::setw(2)<<minute<<":"<<second<<"\n";
    DG_RANK0 std::cout <<"which is         \t"<<t.diff()/p.itstp/p.maxout<<"s/step\n";
#ifdef WITH_MPI
    MPI_Finalize();
#endif //WITH_MPI

    return 0;

}
