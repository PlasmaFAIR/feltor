#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <string>
#include <functional>
#include "json/json.h"

#include "dg/algorithm.h"
#include "dg/geometries/geometries.h"
#include "dg/file/file.h"
#include "feltordiag.h"

thrust::host_vector<float> append( const thrust::host_vector<float>& in, const dg::aRealTopology3d<double>& g)
{
    unsigned size2d = g.n()*g.n()*g.Nx()*g.Ny();
    thrust::host_vector<float> out(g.size()+size2d);
    for( unsigned i=0; i<g.size(); i++)
        out[i] = in[i];
    for( unsigned i=0; i<size2d; i++)
        out[g.size()+i] = in[i];
    return out;
}
//convert all 3d variables of every N-th timestep to float
//and interpolate to a FACTOR times finer grid in phi
//also periodify in 3d and equidistant in RZ
//input should probably come from another json file
int main( int argc, char* argv[])
{
    if( argc != 3)
    {
        std::cerr << "Usage: "<<argv[0]<<" [input.nc] [output.nc]\n";
        return -1;
    }
    std::cout << argv[1]<< " -> "<<argv[2]<<std::endl;

    //------------------------open input nc file--------------------------------//
    dg::file::NC_Error_Handle err;
    int ncid_in;
    err = nc_open( argv[1], NC_NOWRITE, &ncid_in); //open 3d file
    size_t length;
    err = nc_inq_attlen( ncid_in, NC_GLOBAL, "inputfile", &length);
    std::string inputfile(length, 'x');
    err = nc_get_att_text( ncid_in, NC_GLOBAL, "inputfile", &inputfile[0]);
    dg::file::WrappedJsonValue js( dg::file::error::is_warning);
    dg::file::string2Json(inputfile, js.asJson(), dg::file::comments::are_forbidden);
    const feltor::Parameters p(js);
    std::cout << js.asJson().toStyledString() << std::endl;
    dg::geo::TokamakMagneticField mag;
    try{
        mag = dg::geo::createMagneticField(js["magnetic_field"]["params"]);
    }catch(std::runtime_error& e)
    {
        std::cerr << "ERROR in geometry file "<<argv[1]<<std::endl;
        std::cerr <<e.what()<<std::endl;
        return -1;
    }

    //-----------------Create Netcdf output file with attributes----------//
    int ncid_out;
    err = nc_create(argv[2],NC_NETCDF4|NC_CLOBBER, &ncid_out);

    /// Set global attributes
    std::map<std::string, std::string> att;
    att["title"] = "Output file of feltor/src/feltor/interpolate_in_3d.cu";
    att["Conventions"] = "CF-1.7";
    ///Get local time and begin file history
    auto ttt = std::time(nullptr);
    auto tm = *std::localtime(&ttt);
    std::ostringstream oss;
    ///time string  + program-name + args
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    for( int i=0; i<argc; i++) oss << " "<<argv[i];
    att["history"] = oss.str();
    att["comment"] = "Find more info in feltor/src/feltor.tex";
    att["source"] = "FELTOR";
    att["references"] = "https://github.com/feltor-dev/feltor";
    att["inputfile"] = inputfile;
    for( auto pair : att)
        err = nc_put_att_text( ncid_out, NC_GLOBAL,
            pair.first.data(), pair.second.size(), pair.second.data());

    //-------------------Construct grids-------------------------------------//

    const double Rmin=mag.R0()-p.boxscaleRm*mag.params().a();
    const double Zmin=-p.boxscaleZm*mag.params().a();
    const double Rmax=mag.R0()+p.boxscaleRp*mag.params().a();
    const double Zmax=p.boxscaleZp*mag.params().a();
    const unsigned FACTOR = 6;

    unsigned cx = js["output"]["compression"].get(0u,1).asUInt();
    unsigned cy = js["output"]["compression"].get(1u,1).asUInt();
    unsigned n_out = p.n, Nx_out = p.Nx/cx, Ny_out = p.Ny/cy, Nz_out = p.Nz;
    dg::RealCylindricalGrid3d<double> g3d_in( Rmin,Rmax, Zmin,Zmax, 0, 2*M_PI,
        n_out, Nx_out, Ny_out, Nz_out, p.bcxN, p.bcyN, dg::PER);
    dg::RealCylindricalGrid3d<double> g3d_out( Rmin,Rmax, Zmin,Zmax, 0, 2*M_PI,
        n_out, Nx_out, Ny_out, FACTOR*Nz_out, p.bcxN, p.bcyN, dg::PER);
    dg::RealCylindricalGrid3d<double> g3d_out_equidistant( Rmin,Rmax, Zmin,Zmax, 0, 2*M_PI,
        1, n_out*Nx_out, n_out*Ny_out, FACTOR*Nz_out, p.bcxN, p.bcyN, dg::PER);
    dg::RealCylindricalGrid3d<float> g3d_out_periodic( Rmin,Rmax, Zmin,Zmax, 0, 2*M_PI+g3d_out.hz(),
        n_out, Nx_out, Ny_out, FACTOR*Nz_out+1, p.bcxN, p.bcyN, dg::PER);
    dg::RealCylindricalGrid3d<float> g3d_out_periodic_equidistant( Rmin,Rmax, Zmin,Zmax, 0, 2*M_PI+g3d_out.hz(),
        1, n_out*Nx_out, n_out*Ny_out, FACTOR*Nz_out+1, p.bcxN, p.bcyN, dg::PER);

    // Construct weights and temporaries
    dg::HVec transferH_in = dg::evaluate(dg::zero,g3d_in);
    dg::HVec transferH_out = dg::evaluate(dg::zero,g3d_out);
    dg::HVec transferH = dg::evaluate(dg::zero,g3d_out_equidistant);
    dg::fHVec transferH_out_float = dg::construct<dg::fHVec>( transferH);

    // define 4d dimension
    int dim_ids[4], tvarID;
    err = dg::file::define_dimensions( ncid_out, dim_ids, &tvarID,
            g3d_out_periodic_equidistant, {"time", "z", "y", "x"});
    std::map<std::string, int> id4d;

    /////////////////////////////////////////////////////////////////////////
    auto bhat = dg::geo::createBHat( mag);
    dg::geo::Fieldaligned<dg::CylindricalGrid3d, dg::IHMatrix, dg::HVec> fieldaligned(
        bhat, g3d_out, dg::NEU, dg::NEU, dg::geo::NoLimiter(), //let's take NEU bc because N is not homogeneous
        p.rk4eps, 5, 5);
    dg::IHMatrix interpolate_in_2d = dg::create::interpolation( g3d_out_equidistant, g3d_out);


    for( auto& record : feltor::diagnostics3d_static_list)
    {
        if( record.name != "xc" && record.name != "yc" && record.name != "zc" )
        {
            int vID;
            err = nc_def_var( ncid_out, record.name.data(), NC_FLOAT, 3, &dim_ids[1],
                &vID);
            err = nc_put_att_text( ncid_out, vID, "long_name", record.long_name.size(),
                record.long_name.data());

            int dataID = 0;
            err = nc_inq_varid(ncid_in, record.name.data(), &dataID);
            err = nc_get_var_double( ncid_in, dataID, transferH_in.data());
            transferH_out = fieldaligned.interpolate_from_coarse_grid(
                g3d_in, transferH_in);
            dg::blas2::symv( interpolate_in_2d, transferH_out, transferH);
            dg::assign( transferH, transferH_out_float);

            err = nc_enddef( ncid_out);
            err = nc_put_var_float( ncid_out, vID, append(transferH_out_float, g3d_out_equidistant).data());
            err = nc_redef(ncid_out);
        }
    }
    for( auto record : feltor::generate_cyl2cart( g3d_out_equidistant) )
    {
        int vID;
        err = nc_def_var( ncid_out, std::get<0>(record).data(), NC_FLOAT, 3, &dim_ids[1],
            &vID);
        err = nc_put_att_text( ncid_out, vID, "long_name", std::get<1>(record).size(),
            std::get<1>(record).data());
        dg::assign( std::get<2>(record), transferH_out_float);
        err = nc_enddef( ncid_out);
        err = nc_put_var_float( ncid_out, vID, append(transferH_out_float, g3d_out_equidistant).data());
        err = nc_redef(ncid_out);

    }
    for( auto& record : feltor::diagnostics3d_list)
    {
        std::string name = record.name;
        std::string long_name = record.long_name;
        err = nc_def_var( ncid_out, name.data(), NC_FLOAT, 4, dim_ids,
            &id4d[name]);
        err = nc_put_att_text( ncid_out, id4d[name], "long_name", long_name.size(),
            long_name.data());
    }
    err = nc_enddef( ncid_out);



    int timeID;
    double time=0.;

    size_t steps;
    err = nc_inq_unlimdim( ncid_in, &timeID); //Attention: Finds first unlimited dim, which hopefully is time and not energy_time
    err = nc_inq_dimlen( ncid_in, timeID, &steps);
    size_t count3d_in[4]  = {1, g3d_in.Nz(), g3d_in.n()*g3d_in.Ny(), g3d_in.n()*g3d_in.Nx()};
    size_t count3d_out[4] = {1, g3d_out_periodic_equidistant.Nz(), g3d_out_equidistant.n()*g3d_out_equidistant.Ny(), g3d_out_equidistant.n()*g3d_out_equidistant.Nx()};
    size_t start3d[4] = {0, 0, 0, 0};
    ///////////////////////////////////////////////////////////////////////
    for( unsigned i=0; i<steps; i+=10)//timestepping
    {
        std::cout << "Timestep = "<<i<< "/"<<steps;
        start3d[0] = i;
        // read and write time
        err = nc_get_vara_double( ncid_in, timeID, start3d, count3d_in, &time);
        std::cout << "  time = " << time << std::endl;
        start3d[0] = i/10;
        err = nc_put_vara_double( ncid_out, tvarID, start3d, count3d_out, &time);
        for( auto& record : feltor::diagnostics3d_list)
        {
            std::string record_name = record.name;
            int dataID =0;
            bool available = true;
            try{
                err = nc_inq_varid(ncid_in, record.name.data(), &dataID);
            } catch ( dg::file::NC_Error& error)
            {
                if(  i == 0)
                {
                    std::cerr << error.what() <<std::endl;
                    std::cerr << "Offending variable is "<<record.name<<"\n";
                    std::cerr << "Writing zeros ... \n";
                }
                available = false;
            }
            if( available)
            {
                err = nc_get_vara_double( ncid_in, dataID,
                    start3d, count3d_in, transferH_in.data());
                transferH_out = fieldaligned.interpolate_from_coarse_grid(
                    g3d_in, transferH_in);
                dg::blas2::symv( interpolate_in_2d, transferH_out, transferH);
                dg::assign( transferH, transferH_out_float);
            }
            else
            {
                dg::blas1::scal( transferH_out_float, (float)0);
            }
            err = nc_put_vara_float( ncid_out, id4d.at(record.name), start3d,
                count3d_out, append(transferH_out_float, g3d_out_equidistant).data());
        }

    } //end timestepping
    err = nc_close(ncid_in);
    err = nc_close(ncid_out);

    return 0;
}
