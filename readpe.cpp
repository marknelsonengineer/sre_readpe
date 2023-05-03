///////////////////////////////////////////////////////////////////////////////
//         University of Hawaii, College of Engineering
//         readpe - SRE - Spr 2023
//
/// Read a Portable Executable file
///
/// @file   readpe.cpp
/// @author Thanh Ly thanhly@hawaii.edu>
/// @author Mark Nelson <marknels@hawaii.edu>
///////////////////////////////////////////////////////////////////////////////

#include <iostream>  // For cout cerr endl
#include <fstream>   // For file_path_
#include <vector>    // For vector
#include <map>       // For map
#include <iomanip>   // For setfill()
#include <string>    // For string

#include <boost/algorithm/string/trim.hpp>  // For boost::algorithm::trim_copy()
#include <boost/core/typeinfo.hpp>  // For boost::core::demangle()

using namespace std;

class DOSFieldMap;    // Forward declaration
class COFF_FieldMap;  // Forward declaration

typedef uint8_t Rules;  ///< The base-type of our rules flag.

#define AS_DEC    0x01  ///< Print the value as a decimal number
#define AS_HEX    0x02  ///< Print the value as a hexadecimal number
#define AS_CHAR   0x04  ///< Print as a fixed-width character array
#define WITH_TIME 0x08  ///< Print with timestamp


/// FieldBase is an any-type base class for Field.
class FieldBase {
public:
   /// Constructor that takes a label, offset and description.
   ///
   /// @param new_offset      The offset into the file's section to the start of the value stored in the file
   /// @param new_description The description of this Field
   /// @param new_rules       Encode special processing rules for this Field
   FieldBase(const size_t     new_offset
            ,const string     new_description
            ,const Rules      new_rules )
            :offset_      ( new_offset )                                      // Member initialization
            ,description_ { boost::algorithm::trim_copy( new_description ) }  // List initialization
            ,rules_       ( new_rules )                                       // Member initialization
   {}

   /// @return The offset to this Field (relative to the start of this group of fields)
   virtual size_t get_offset() const {
      return offset_;
   }

   /// @return The description for this Field
   virtual string get_description() const {
      return description_;
   }

   /// We don't really need the type, what we really want is the string...
   ///
   /// @return The type of Field (as a string)
   virtual const char* get_type() const = 0;

   /// @return `true` if this Field is healthy.  `false` if there's a problem.
   virtual bool validate() const {
      // Nothing to validate for offset_

      if( description_.empty() ) {
         return false;
      }

      return true;
   }

   /// We don't really want the value, what we really want is the string version of the value...
   ///
   /// @return The value of the Field (as a string)
   virtual string get_value() const = 0;

   /// Read the PE File and set the Field value
   ///
   /// @param file_buffer The contents of the PE File
   /// @param file_offset The file offset to this group of fields (not necessarily this particular field)
   virtual void set_value( vector<char>& file_buffer, size_t file_offset ) = 0;

protected:
   size_t      offset_;      ///< The offset into a section to find this Field
   string description_; ///< A description of this Field
   Rules       rules_;       ///< Encode special processing rules for this Field
};


/// Field is a template that creates classes to store a Field with a specified type T.
template <typename T>
class Field : public FieldBase {
   friend DOSFieldMap;     ///< DOSFieldMap needs to directly access `field_` for validation
   friend COFF_FieldMap;   ///< COFF_FieldMap needs to directly access `field_` for validation

public:
   /// Constructor that takes an offset, description and rules.
   ///
   /// @param new_offset      The offset into the file's section to the start of the value stored in the file
   /// @param new_description The description of this Field
   /// @param new_rules       The decorator rules for this Field
   Field(   const size_t      new_offset
           ,const string new_description
           ,const Rules       new_rules
   ) : FieldBase( new_offset, new_description, new_rules )
           ,value_       { T() }
   {}

   /// @return The current value of the Field (as a string).
   string get_value() const override {
      stringstream resultString;

      if( rules_ & AS_DEC ) {
         resultString << dec << value_ << " ";
      }

      if( rules_ & AS_HEX ) {
         if( value_ == 0 ) {
            resultString << 0 << " ";
         } else {
            resultString << "0x" << hex << value_ << " ";
         }
      }

      if( rules_ & AS_CHAR ) {
         resultString << "(";
         for( size_t i = 0 ; i < sizeof( value_ ) ; i++ ) {
            resultString << *(((char*)&value_) + i);
         }
         resultString << ")";
      }

      if( rules_ & WITH_TIME ) {
         resultString << "(";
         time_t t = value_;
         tm tm = *gmtime(&t);
         resultString << put_time(&tm, "%c %Z");
         resultString << ")";
      }

      return resultString.str();
   }

   /// Set the value of the Field
   ///
   /// @param file_buffer The contents of the PE File
   /// @param file_offset The offset to this header in the file
   virtual void set_value( vector<char>& file_buffer, size_t file_offset ) override {
      memcpy(&value_, &file_buffer[file_offset + offset_], sizeof(value_));
   }

   /// @return Get the type of this Field (as a string)
   virtual const char* get_type() const override {
      return typeid(T).name();
   }

protected:
   T value_;  ///< The value of this Field
};


/// A Map of Field objects
class FieldMap : public map<string, FieldBase*> {
public:
   /// Set the offset into the file where this collection of fields starts
   ///
   /// @param new_offset The offset into the file where this collection of fields starts
   virtual void set_file_offset( size_t new_offset ) {
      file_offset_ = new_offset;
   }

   /// @return The offset into the file where this collection of fields starts
   virtual size_t get_file_offset() {
      return file_offset_;
   }

   /// Validate each of the fields in this Map
   ///
   /// @return `true` if everything is valid.  `false` if there's a problem.
   virtual bool validate() const {
      for (const auto& [label, field] : *this ) {
         if( !field->validate() ) {
            return false;
         }
      }

      // There's nothing to validate for a generic map

      // There's nothing to validate for file_offset_

      return true;
   }

   /// Parse data out of PEFile to populate the Fields
   ///
   /// @param file_buffer A reference to the file's contents
   virtual void parse( vector<char>& file_buffer ) {
      for (const auto& [label, field] : *this ) {
         field->set_value( file_buffer, file_offset_ );
      }
   }

   /// Print this FieldMap
   virtual void print() const {
      for (const auto& [label, field] : *this ) {
         string valueAsString = field->get_value();

         if( valueAsString.empty() ) {  // If it's empty, then skip it
            continue;                   // We may need to bring in a field
         }                              // for validation that we don't want to print

         cout << "    "
                   << setfill( ' ' )  /* Space pad    */
                   << left            /* Left justify */
                   << setw(34) << field->get_description() + ":"
                   << field->get_value()
                   << endl ;
      }
   }

protected:
   /// The offset into the file where this collection of fields starts
   size_t file_offset_ = 0;
};


/// A DOS-specific FieldMap
///
/// @see offset reference http://www.sunshine2k.de/reversing/tuts/tut_pe.htm
class DOSFieldMap : public FieldMap {
public:
   /// Create a new DOSFieldMAp at `new_file_offset`
   ///
   /// @param new_file_offset The offset into the file for this collection of fields
   DOSFieldMap( const size_t new_file_offset ) {
      file_offset_ = new_file_offset;

      this->insert( { "01_dos_e_magic",    new Field<uint16_t>( 0x00, "Magic number"                , AS_HEX | AS_CHAR ) } );
      this->insert( { "02_dos_e_cblp",     new Field<uint16_t>( 0x02, "Bytes in last page"          , AS_DEC           ) } );
      this->insert( { "03_dos_e_cp",       new Field<uint16_t>( 0x04, "Pages in file"               , AS_DEC           ) } );
      this->insert( { "04_dos_e_crlc",     new Field<uint16_t>( 0x06, "Relocations"                 , AS_DEC           ) } );
      this->insert( { "05_dos_e_cparhdr",  new Field<uint16_t>( 0x08, "Size of header in paragraphs", AS_DEC           ) } );
      this->insert( { "06_dos_e_minalloc", new Field<uint16_t>( 0x0A, "Minimum extra paragraphs"    , AS_DEC           ) } );
      this->insert( { "07_dos_e_maxalloc", new Field<uint16_t>( 0x0C, "Maximum extra paragraphs"    , AS_DEC           ) } );
      this->insert( { "08_dos_e_ss",       new Field<uint16_t>( 0x0E, "Initial (relative) SS value" , AS_DEC           ) } );
      this->insert( { "09_dos_e_sp",       new Field<uint16_t>( 0x10, "Initial SP value"            , AS_HEX           ) } );
      this->insert( { "10_dos_e_ip",       new Field<uint16_t>( 0x14, "Initial IP value"            , AS_HEX           ) } );
      this->insert( { "11_dos_e_cs",       new Field<uint16_t>( 0x16, "Initial (relative) CS value" , AS_HEX           ) } );
      this->insert( { "12_dos_e_lfarlc",   new Field<uint16_t>( 0x18, "Address of relocation table" , AS_HEX           ) } );
      this->insert( { "13_dos_e_ovno",     new Field<uint16_t>( 0x1A, "Overlay number"              , AS_DEC           ) } );
      this->insert( { "14_dos_e_oemid",    new Field<uint16_t>( 0x24, "OEM identifier"              , AS_DEC           ) } );
      this->insert( { "15_dos_e_oeminfo",  new Field<uint16_t>( 0x26, "OEM information"             , AS_DEC           ) } );
      this->insert( { "16_dos_e_lfanew",   new Field<uint32_t>( 0x3C, "PE header offset"            , AS_HEX           ) } );
   }
   /// @return The file offset to the COFF section
   uint32_t get_exe_header_offset() {
      return dynamic_cast<Field<uint32_t>*>(this->at( "16_dos_e_lfanew" ))->value_;
   }

   /// @return `true` if the DOSFieldMap is valid
   virtual bool validate() const {
      if( !FieldMap::validate() ) {
         return false;
      }

      if( this->at("01_dos_e_magic")->get_value() != "0x5a4d (MZ)" ) { // Validate the magic is "MZ"
         return false;
      }

      return true ;
   }

   /// Print the DOSFieldMap
   virtual void print() const {
      cout << "DOS Header" << endl;
      FieldMap::print();
   }
};


/// A COFF-specific FieldMap
class COFF_FieldMap : public FieldMap {
public:
   /// Create a new COFF_FieldMAp at `new_file_offset`
   ///
   /// @param new_file_offset The offset into the file for this collection of fields
   COFF_FieldMap( const size_t new_file_offset ) {
      file_offset_ = new_file_offset;

      this->insert( { "01_coff_signature",            new Field<uint32_t>( 0x00, "coff_signature"          , 0 ) } );
      this->insert( { "02_coff_machine",              new Field<uint16_t>( 0x04, "Machine"                 , AS_HEX ) } );
      this->insert( { "03_coff_sections",             new Field<uint16_t>( 0x06, "Number of Sections"      , AS_DEC ) } );
      this->insert( { "04_coff_timedatestamp",        new Field<uint32_t>( 0x08, "Date/time stamp"         , AS_DEC | WITH_TIME ) } );
      this->insert( { "05_coff_PointerToSymbolTable", new Field<uint32_t>( 0x0C, "Symbol Table offset"     , AS_DEC ) } );
      this->insert( { "06_coff_NumberOfSymbols",      new Field<uint32_t>( 0x10, "Number of symbols"       , AS_DEC ) } );
      this->insert( { "07_coff_SizeOfOptionalHeader", new Field<uint16_t>( 0x14, "Size of optional header" , AS_HEX ) } );
      this->insert( { "08_coff_characteristics",      new Field<uint16_t>( 0x16, "Characteristics"         , AS_HEX ) } );
   }

   /// @return The file offset to the top of the section table
   uint32_t get_section_table_offset() {
      // The first section starts immediately after the optional header...
      // So, it's at: file_offset_ + 18 (size of the COFF header) + coff_SizeOfOptionalHeader
      return file_offset_ + 0x18 + dynamic_cast<Field<uint32_t>*>(this->at( "07_coff_SizeOfOptionalHeader" ))->value_;
   }

   /// @todo Probably also need to implement a get_number_of_sections() method

   /// @return `true` if the COFF_FieldMap is valid
   virtual bool validate() const {
      if( !FieldMap::validate() ) {
         return false;
      }

      uint16_t signature = dynamic_cast<Field<uint32_t>*>(this->at( "01_coff_signature" ))->value_;
      if( signature != 0x4550 ) { // Validate the magic is "PE"
         return false;
      }

      return true ;
   }

   /// Print the COFF_FieldMap
   virtual void print() const {
      cout << "COFF/File header" << endl;
      FieldMap::print();
   }
};


/// This class represents a PEFile
class PEFile {
public:
   /// Read the PEFile at `new_file_path`
   ///
   /// @param new_file_path The name of the PE file to process
   PEFile(const string& new_file_path) : file_path_(new_file_path) {
      ifstream file(file_path_, ios::binary); //TODO: This is not working
      if (!file.is_open()) {
         throw runtime_error("Failed to open the file: " + file_path_);
      }

      file.seekg(0, ios::end);
      file_size_ = file.tellg();
      file.seekg(0, ios::beg);

      buffer_.resize(file_size_);
      file.read(buffer_.data(), file_size_);
      file.close();
   }

   /// Print the headers and sections of this PE File
   virtual void print() {
      dos_field_map_.parse( buffer_ );
      if( !dos_field_map_.validate() ) {
         cout << "The DOS header is invalid" << endl;
         exit( 1 );
      }
      dos_field_map_.print();

      uint32_t coff_offset = dos_field_map_.get_exe_header_offset();
      (void) coff_offset;

      COFF_FieldMap coff_header_map { coff_offset };
      coff_header_map.parse( buffer_ );
      if( !coff_header_map.validate() ) {
         cout << "The COFF header is invalid" << endl;
         exit( 1 );
      }
      coff_header_map.print();
   }

protected:
   string file_path_;     ///< The name of the PE file
   size_t file_size_;          ///< The size of the PE file
   vector<char> buffer_;  ///< The contents of the PE file

   DOSFieldMap dos_field_map_ {0};  ///< The collection of DOS Fields
};


/// Main entry point for readpe

/// @param argc The number of arguments
/// @param argv An array of arguments as strings
/// @return The result code for this program
int main( int argc, char* argv[] ) {
    if( argc <= 1 ) {
       cout << "Usage:  readpe PEfile" << endl;
    }

    /// @todo Convert into a for() loop and process all of the files on the command line
    PEFile pe_file( argv[1] );
    pe_file.print();

    return 0;
}
