#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <stdexcept> //std::runtime_error
#include <nlohmann/json.hpp>

/*!@file
 *
 * Json utility functions
 */

//Note that the json utilities are separate from netcdf utilities because
//of the different library dependencies that they incur
namespace dg
{
namespace file
{
/**
 * @defgroup json Json utilities
 * \#include "dg/file/json_utilities.h"
 *
 * @addtogroup json
 * @{
 */

///@brief Switch between how to handle errors in a Json utitlity functions
enum class error{
    is_throw, //!< throw an error
    is_warning, //!< Handle the error by writing a warning to \c std::cerr
    is_silent //!< Ignore the error and silently continue execution
};

///@brief Switch how comments are treated in a json string or file
enum class comments{
    are_kept, //!< Keep comments in the Json value
    are_discarded, //!< Allow comments but discard them in the Json value
    are_forbidden //!< Treat comments as invalid Json
};

/**
 * @brief Wrapped Access to Json values with error handling
 *
 * The purpose of this class is to wrap the
 * access to a nlohmann::json with guards that raise exceptions or display
 * warnings in case an error occurs, for example when a key is misspelled,
 * missing or has the wrong type.
 * The goal is the composition of a good error message that helps a user
 * quickly debug the input (file).
 *
 * The Wrapper is necessary because json by default silently
 * generates a new key in case it is not present which in our scenario is an
 * invitation for stupid mistakes.
 *
 * You can use the \c WrappedJsonValue like a \c nlohmann::json with read-only access:
 * @code
dg::file::WrappedJsonValue ws = dg::file::file2Json("test.json");
try{
    std::string hello = ws.get( "hello", "").asString();
    // the following access will throw
    int idx0 = ws[ "array" ][out_of_bounds_index].asInt();
} catch ( std::exception& e){
    std::cerr << "Error in file test.json\n";
    //the what string knows that the out of bounds error occured in the array
    //called "array"
    std::cerr << e.what()<<std::endl;
}
 * @endcode
 * A feature of the class is that it keeps track of how a value is called.
 * For example
 * @code
void some_function( dg::file::WrappedJsonValue ws)
{
    int value = ws[ "some_non_existent_key"].asUInt();
}

dg::file::WrappedJsonValue js;
try{
    some_function( js["nested"]);
} catch ( std::exception& e){ std::cerr << e.what()<<std::endl; }
//The what string knows that "some_non_existent_key" is expected to be
//contained in the "nested" key.
 * @endcode
 */
struct WrappedJsonValue
{
    ///@brief Default constructor
    ///By default the error mode is \c error::is_throw
    WrappedJsonValue() : m_js(0), m_mode( error::is_throw){}
    ///@brief Construct with error mode
    ///@param mode The error mode
    WrappedJsonValue( error mode): m_js(0), m_mode( mode) {}
    ///@brief By default the error mode is \c error::is_throw
    ///@param js The Json value that will be guarded
    WrappedJsonValue(nlohmann::json js): m_js(js), m_mode( error::is_throw) {}
    ///@brief Construct with Json value and error mode
    ///@param js The Json value that will be guarded
    ///@param mode The error mode
    WrappedJsonValue(nlohmann::json js, error mode): m_js(js), m_mode( mode) {}
    ///@brief Change the error mode
    ///@param new_mode The new error mode
    void set_mode( error new_mode){
        m_mode = new_mode;
    }
    ///Read access to the raw Json value
    const nlohmann::json& asJson( ) const{ return m_js;}
    ///Write access to the raw Json value (if you know what you are doing)
    nlohmann::json& asJson( ) { return m_js;}

    /*! @brief The creation history of the object
     *
     * Useful to print when debugging parameter files
     * @return A string containing object history
     */
    std::string access_string() const {return m_access_str;}

    // //////////Members imitating the original nlohmann::json///////////////
    /// Wrap the corresponding nlohmann::json function with error handling
    WrappedJsonValue operator[](std::string key) const{
        return get( key, nlohmann::json::object(), "empty object ");
    }
    /// Wrap the corresponding nlohmann::json function with error handling
    WrappedJsonValue get( std::string key, const nlohmann::json& value) const{
        std::stringstream default_str;
        default_str << "value "<<value;
        return get( key, value, default_str.str());
    }
    /// Wrap the corresponding nlohmann::json function with error handling
    WrappedJsonValue operator[]( unsigned idx) const{
        return get( idx, nlohmann::json::object(), "empty array");
    }
    /// Wrap the corresponding nlohmann::json function with error handling
    WrappedJsonValue get( unsigned idx, const nlohmann::json& value) const{
        std::stringstream default_str;
        default_str << "value "<<value;
        return get( idx, value, default_str.str());
    }
    /// Wrap the corresponding nlohmann::json function with error handling
    unsigned size() const{
        return m_js.size();
    }
    /// Wrap the corresponding nlohmann::json function with error handling
    double asDouble( double value = 0) const{
        if( m_js.is_number())
            return m_js.template get<double>();
        return type_error<double>( value, "a Double");
    }
    /// Wrap the corresponding nlohmann::json function with error handling
    unsigned asUInt( unsigned value = 0) const{
        if( m_js.is_number())
            return m_js.template get<unsigned>();
        return type_error<unsigned>( value, "an Unsigned");
    }
    /// Wrap the corresponding nlohmann::json function with error handling
    int asInt( int value = 0) const{
        if( m_js.is_number())
            return m_js.template get<int>();
        return type_error<int>( value, "an Int");
    }
    /// Wrap the corresponding nlohmann::json function with error handling
    bool asBool( bool value = false) const{
        if( m_js.is_boolean())
            return m_js.template get<bool>();
        return type_error<bool>( value, "a Bool");
    }
    /// Wrap the corresponding nlohmann::json function with error handling
    std::string asString( std::string value = "") const{
        //return m_js["hhaha"].asString(); //does not throw
        if( m_js.is_string())
            return m_js.template get<std::string>();
        return type_error<std::string>( value, "a String");
    }
    private:
    WrappedJsonValue(nlohmann::json js, error mode, std::string access):m_js(js), m_mode( mode), m_access_str(access) {}
    WrappedJsonValue get( std::string key, const nlohmann::json& value, std::string default_str) const
    {
        std::string access = m_access_str + "\""+key+"\": ";
        std::stringstream message;
        if( !m_js.is_object( ) || !m_js.contains(key))
        {
            message <<"*** Key error: "<<access<<" not found.";
            raise_error( message.str(), default_str);
            return WrappedJsonValue( value, m_mode, access);
        }
        return WrappedJsonValue(m_js[key], m_mode, access);
    }
    WrappedJsonValue get( unsigned idx, const nlohmann::json& value, std::string default_str) const
    {
        std::string access = m_access_str + "["+std::to_string(idx)+"] ";
        if( !m_js.is_array() || !(idx < m_js.size()))
        {
            std::stringstream message;
            //if( !m_js.isArray())
            //    message <<"*** Key error: "<<m_access_str<<" is not an Array.";
            //else
            if( m_access_str.empty())
                message <<"*** Index error: Index "<<idx<<" not present.";
            else
                message <<"*** Index error: Index "<<idx<<" not present in "<<m_access_str<<".";
            raise_error( message.str(), default_str);
            return WrappedJsonValue( value, m_mode, access);
        }
        return WrappedJsonValue(m_js[idx], m_mode, access);
    }
    template<class T>
    T type_error( T value, std::string type) const
    {
        std::stringstream message, default_str;
        default_str << "value "<<value;
        message <<"*** Type error: "<<m_access_str<<" "<<m_js<<" is not "<<type<<".";
        raise_error( message.str(), default_str.str());
        return value;
    }
    void raise_error( std::string message, std::string default_str) const
    {
        if( error::is_throw == m_mode)
            throw std::runtime_error( message);
        else if ( error::is_warning == m_mode)
            std::cerr <<"WARNING "<< message<<" Using default "<<default_str<<"\n";
        else
            ;
    }
    nlohmann::json m_js;
    error m_mode;
    std::string m_access_str = "";
};

/**
 * @brief Convenience wrapper to open a file and parse it into a nlohmann::json
 *
 * @note included in \c json_utilities.h
 * @param filename Name of the JSON file to parse
 * @param comm determines the handling of comments in the Json file
 * @param err determines how parser errors are handled by the function
 * \c error::is_throw:  throw a \c std::runtime_error containing an error message on any error that occurs on parsing;
 * \c error::is_warning: write the error message to std::cerr and return;
 * \c error::is_silent: silently return
 * @return Contains all the found Json variables on output (error mode is \c err)
 */
static inline nlohmann::json file2Json(std::string filename, enum comments comm = file::comments::are_discarded, enum error err = file::error::is_throw)
{

    std::ifstream isI( filename);
    if( !isI.good())
    {
        std::string message = "\nAn error occured while parsing "+filename+"\n";
        message +=  "*** File does not exist! *** \n\n";
        if( err == error::is_throw)
            throw std::runtime_error( message);
        else if (err == error::is_warning)
            std::cerr << "WARNING: "<<message<<std::endl;
        else
            ;
        return nlohmann::json();
    }
    bool ignore_comments = false, allow_exceptions = false;
    if ( comm == file::comments::are_discarded)
        ignore_comments =  true;
    if ( err == error::is_throw)
        allow_exceptions = true; //throws nlohmann::json::parse_error


    nlohmann::json js = nlohmann::json::parse( isI, nullptr, allow_exceptions, ignore_comments);
    if( !allow_exceptions && err == error::is_warning && js.is_discarded())
    {
        std::string message = "An error occured while parsing "+filename+"\n";
        std::cerr << "WARNING: "<<message<<std::endl;
    }
    return js;
}
/**
 * @brief Convenience wrapper to parse a string into a nlohmann::json
 *
 * Parse a string into a Json Value
 * @attention This function will throw a \c std::runtime_error with the Json error string on any error that occurs on parsing.
 * @note included in \c json_utilities.h
 * @param input The string to interpret as a Json string
 * @param comm determines the handling of comments in the Json string
 * @param err determines how parser errors are handled by the function
 * \c error::is_throw:  throw a \c std::runtime_error containing an error message on any error that occurs on parsing;
 * \c error::is_warning: write the error message to std::cerr and return;
 * \c error::is_silent: silently return
 * @return Contains all the found Json variables on output
 */
static inline nlohmann::json string2Json(std::string input, enum comments comm = file::comments::are_discarded, enum error err = file::error::is_throw)
{
    bool ignore_comments = false, allow_exceptions = false;
    if ( comm == file::comments::are_discarded)
        ignore_comments =  true;
    if ( err == error::is_throw)
        allow_exceptions = true; //throws nlohmann::json::parse_error


    nlohmann::json js = nlohmann::json::parse( input, nullptr, allow_exceptions, ignore_comments);
    if( !allow_exceptions && err == error::is_warning && js.is_discarded())
    {
        std::string message = "An error occured while parsing \n";
        std::cerr << "WARNING: "<<message<<std::endl;
    }
    return js;
}

///@}
}//namespace file
}//namespace dg
