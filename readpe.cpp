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

#include <algorithm> // For all_of()
#include <execution> // For execution::par
#include <fstream>   // For file_path_
#include <iomanip>   // For setfill()
#include <iostream>  // For cout cerr endl
#include <map>       // For map
#include <string>    // For string
#include <vector>    // For vector

#include <boost/algorithm/string/trim.hpp>  // For boost::algorithm::trim_copy()
#include <boost/core/typeinfo.hpp>          // For boost::core::demangle()

using namespace std;

class DOS_FieldMap;   // Forward declaration
class COFF_FieldMap;  // Forward declaration

typedef uint8_t Rules;  ///< The base-type of our rules flag.

// Special processing rules
#define AS_DEC     0x01  ///< Print the value as a decimal number
#define AS_HEX     0x02  ///< Print the value as a hexadecimal number
#define AS_CHAR    0x04  ///< Print as a fixed-width character array
#define WITH_TIME  0x08  ///< Print with timestamp
#define WITH_FLAG  0x10  ///< Decode a single flag
#define WITH_FLAGS 0x20  ///< Decode several flags

/// The relationship between the field, flag and the printed value
map<pair<string, uint32_t>, string> flags {
         { pair( "02_coff_machine",                0x0000 ), "IMAGE_FILE_MACHINE_UNKNOWN"          }
        ,{ pair( "02_coff_machine",                0x8664 ), "IMAGE_FILE_MACHINE_AMD64"            }
        ,{ pair( "02_coff_machine",                0x014c ), "IMAGE_FILE_MACHINE_I386"             }
        ,{ pair( "02_coff_machine",                0xaa64 ), "IMAGE_FILE_MACHINE_ARM64"            }
        ,{ pair( "02_coff_machine",                0x0200 ), "IMAGE_FILE_MACHINE_IA64"             }
        ,{ pair( "08_coff_characteristics",        0x0002 ), "IMAGE_FILE_EXECUTABLE_IMAGE"         }
        ,{ pair( "08_coff_characteristics",        0x0020 ), "IMAGE_FILE_LARGE_ADDRESS_AWARE"      }
        ,{ pair( "08_coff_characteristics",        0x0100 ), "IMAGE_FILE_32BIT_MACHINE"            }
        ,{ pair( "08_coff_characteristics",        0x2000 ), "IMAGE_DLLCHARACTERISTICS_WDM_DRIVER" }
        ,{ pair( "07_section_characteristics", 0x00000020 ), "IMAGE_SCN_CNT_CODE"                  }
        ,{ pair( "07_section_characteristics", 0x00000040 ), "IMAGE_SCN_CNT_INITIALIZED_DATA"      }
        ,{ pair( "07_section_characteristics", 0x02000000 ), "IMAGE_SCN_MEM_DISCARDABLE"           }
        ,{ pair( "07_section_characteristics", 0x04000000 ), "IMAGE_SCN_MEM_NOT_CACHED"            }
        ,{ pair( "07_section_characteristics", 0x08000000 ), "IMAGE_SCN_MEM_NOT_PAGED"             }
        ,{ pair( "07_section_characteristics", 0x10000000 ), "IMAGE_SCN_MEM_SHARED"                }
        ,{ pair( "07_section_characteristics", 0x20000000 ), "IMAGE_SCN_MEM_EXECUTE"               }
        ,{ pair( "07_section_characteristics", 0x40000000 ), "IMAGE_SCN_MEM_READ"                  }
        ,{ pair( "07_section_characteristics", 0x80000000 ), "IMAGE_SCN_MEM_WRITE"                 }
}; // flags


/// FieldBase is an any-type base class for Field.
class FieldBase {
protected:
   size_t offset_;      ///< The offset into this section of fields
   string description_; ///< A description of this Field
   Rules  rules_;       ///< Special processing rules for this Field such as #AS_HEX or #WITH_TIME

public:
   /// Construct a FieldBase with an offset, description and rules.
   FieldBase(const size_t  new_offset       ///< The offset into this section of fields
            ,const string& new_description  ///< The description of this Field
            ,const Rules   new_rules )      ///< Special processing rules for this Field such as #AS_HEX or #WITH_TIME
            :offset_      ( new_offset )        // Member initialization
            ,description_ ( new_description )   // Member initialization
            ,rules_       ( new_rules )         // Member initialization
   {}


   virtual size_t get_offset() const {
      return offset_;  /// @return Field.offset_ (The offset is relative to the start of this group of fields)
   }

   virtual string get_description() const {
      return description_;  /// @return A description for this Field
   }

   virtual Rules get_rules() const {
      return rules_;  /// @return Special processing rules for this Field
   }

   /// @return `true` if this Field is healthy.  `false` if there's a problem.
   virtual bool validate() const {
      /// Nothing to validate for #offset_ and #rules_
      if( description_.empty() ) { return false; }
      return true;
   }

   /// We don't really want Field.value_... we really want the value as a `string`!
   virtual string get_value() const = 0;  ///< @return The value of Field.value_ (as a string)

   /// Extract bytes from PEFile.buffer_ using `file_offset` and #offset_ and set Field.value_
   virtual void set_value(
           vector<char>& file_buffer         ///< A pointer to PEFile.buffer_
          ,size_t        file_offset ) = 0;  ///< The PEFile.buffer_ offset to the start of this group of fields (not necessarily this particular field)

   /// Print the characteristics #flags
   /// @param label The field to search in the #flags map
   virtual void print_characteristics(
           string label ) const = 0;
}; // FieldBase


/// Field is a template derived from FieldBase that holds fields with a specific type
template <typename T>
class Field : public FieldBase {
   friend DOS_FieldMap;   ///< Directly accesses Field.value_ for validation
   friend COFF_FieldMap;  ///< Directly accesses Field.value_ for validation

protected:
   T value_;  ///< The value of this Field

public:
   /// Construct a Field with an offset, description and rules.
   Field(   const size_t  new_offset       ///< The offset into this section of fields
           ,const string& new_description  ///< The description of this Field
           ,const Rules   new_rules        ///< Special processing rules for this Field such as #AS_HEX or #WITH_TIME
   ) : FieldBase( new_offset, new_description, new_rules )
           ,value_       { T() }
   {}

   string get_value() const override {
      stringstream resultString;

      stringstream hexString;
      if( value_ == 0 ) {
         hexString << 0 << " ";
      } else {
         hexString << "0x" << hex << value_;
      }

      stringstream charString;
      for( size_t i { 0 } ; i < sizeof( value_ ) ; i++ ) {
         charString << *(((char*)&value_) + i);
      }

      if( rules_ & AS_HEX && rules_ & AS_CHAR ) {
         resultString << hexString.str() << " (" << charString.str() << ")";
      } else if( rules_ & AS_DEC && rules_ & AS_HEX ) {
         resultString << hexString.str() << " (" << dec << value_ << " bytes)";
      } else if( rules_ & AS_DEC ) {
         resultString << dec << value_ << " ";
      } else if( rules_ & AS_HEX ) {
         resultString << hexString.str();
      } else if( rules_ & AS_CHAR ) {
         resultString << charString.str();
      }

      if( rules_ & WITH_TIME ) {
         const time_t timestamp = value_;
         resultString << "(" << put_time(gmtime( &timestamp ), "%c %Z") << ")";
      }

      if( rules_ & WITH_FLAG ) {
         try {
            resultString << " " << flags.at( pair( "02_coff_machine", value_) );
         } catch( const out_of_range& ) {
            resultString << "UNKNOWN FLAG MAPPING";
         }
      }

      return resultString.str();
   } // get_value()

   virtual void set_value(
           vector<char>& file_buffer
          ,size_t file_offset
          ) override {
      memcpy( &value_, &file_buffer[file_offset + offset_], sizeof(value_) );
   }

   virtual void print_characteristics( const string label ) const override {
      cout << "    Characteristics names" << endl;

      for( size_t i { 0 } ; i < sizeof( T )*8 ; i++ ) {
         T mask = 1 << i;
         if( value_ & mask ) {
            cout << std::setw(42) << std::setfill( ' ' ) << "";

            try {
               cout << flags.at( pair( label, mask ) );
            } catch( const out_of_range& ) {
               cout << "UNKNOWN FLAG MAPPING: " << hex << "0x" << mask;
            }
            cout << endl;
         }
      }
   } // print_characteristics()
}; // Field


/// A generic Map of Field objects
class FieldMap : public map<string, unique_ptr<FieldBase>> {
protected:
   size_t file_offset_ { 0 };  ///< Offset into PEFile.buffer_ where this group of fields start

public:
   /// Validate each Field in this Map (generic)
   /// There's nothing to validate for a generic `map` nor #file_offset_
   virtual bool validate() const {  /// This is a parallel iterator that uses a Lambda expression
      return all_of( execution::par, this->cbegin(), this->cend(),
                     [](const auto& fieldBase) { return fieldBase.second->validate(); }
      );  /// @return `all_of()` returns `true` when the Lambda expression returns `true` for **all** of the elements in the range
   }

   /// Parse data from PEFile.buffer_ to populate Field.value_
   /// @param file_buffer Pointer to PEFile.buffer_
   virtual void parse( vector<char>& file_buffer ) {
      for (const auto& [label, field] : *this ) {
         (*field).set_value( file_buffer, file_offset_ );
      }
   }

   /// Print this FieldMap (generic)
   virtual void print() const {
      for (const auto& [label, field] : *this ) {
         const string valueAsString = (*field).get_value();

         if( valueAsString.empty() ) {  // If it's empty, then skip it
            continue;                   // We may need to bring in a field
         }                              // for validation that we don't want to print

         cout << "    " << setfill( ' ' )  // Space pad
                        << left            // Left justify
                        << setw(34) << (*field).get_description() + ":"
                        << (*field).get_value()
                        << endl ;

         if( (*field).get_rules() & WITH_FLAGS ) {
            (*field).print_characteristics( label );
         }
      }
   } // print()
}; // FieldMap


/// A DOS-specific FieldMap
///
/// @see DOS header reference: http://www.sunshine2k.de/reversing/tuts/tut_pe.htm
class DOS_FieldMap : public FieldMap {
public:
   /// Create a new DOS_FieldMap
   DOS_FieldMap() {
      file_offset_ = 0;

      this->insert( { "01_dos_e_magic",    make_unique<Field<uint16_t>>( 0x00, "Magic number"                , AS_HEX | AS_CHAR ) } );
      this->insert( { "02_dos_e_cblp",     make_unique<Field<uint16_t>>( 0x02, "Bytes in last page"          , AS_DEC           ) } );
      this->insert( { "03_dos_e_cp",       make_unique<Field<uint16_t>>( 0x04, "Pages in file"               , AS_DEC           ) } );
      this->insert( { "04_dos_e_crlc",     make_unique<Field<uint16_t>>( 0x06, "Relocations"                 , AS_DEC           ) } );
      this->insert( { "05_dos_e_cparhdr",  make_unique<Field<uint16_t>>( 0x08, "Size of header in paragraphs", AS_DEC           ) } );
      this->insert( { "06_dos_e_minalloc", make_unique<Field<uint16_t>>( 0x0A, "Minimum extra paragraphs"    , AS_DEC           ) } );
      this->insert( { "07_dos_e_maxalloc", make_unique<Field<uint16_t>>( 0x0C, "Maximum extra paragraphs"    , AS_DEC           ) } );
      this->insert( { "08_dos_e_ss",       make_unique<Field<uint16_t>>( 0x0E, "Initial (relative) SS value" , AS_DEC           ) } );
      this->insert( { "09_dos_e_sp",       make_unique<Field<uint16_t>>( 0x10, "Initial SP value"            , AS_HEX           ) } );
      this->insert( { "10_dos_e_ip",       make_unique<Field<uint16_t>>( 0x14, "Initial IP value"            , AS_HEX           ) } );
      this->insert( { "11_dos_e_cs",       make_unique<Field<uint16_t>>( 0x16, "Initial (relative) CS value" , AS_HEX           ) } );
      this->insert( { "12_dos_e_lfarlc",   make_unique<Field<uint16_t>>( 0x18, "Address of relocation table" , AS_HEX           ) } );
      this->insert( { "13_dos_e_ovno",     make_unique<Field<uint16_t>>( 0x1A, "Overlay number"              , AS_DEC           ) } );
      this->insert( { "14_dos_e_oemid",    make_unique<Field<uint16_t>>( 0x24, "OEM identifier"              , AS_DEC           ) } );
      this->insert( { "15_dos_e_oeminfo",  make_unique<Field<uint16_t>>( 0x26, "OEM information"             , AS_DEC           ) } );
      this->insert( { "16_dos_e_lfanew",   make_unique<Field<uint32_t>>( 0x3C, "PE header offset"            , AS_HEX           ) } );
   } // DOS_FieldMap()

   /// @return The PEFile.buffer_ offset to the COFF section
   uint32_t get_exe_header_offset() {
      return dynamic_cast<Field<uint32_t>&>( *this->at( "16_dos_e_lfanew" ) ).value_;
   }

   virtual bool validate() const {
      if( !FieldMap::validate() ) { return false; }

      if( this->at("01_dos_e_magic")->get_value() != "0x5a4d (MZ)" ) { // Validate the magic is "MZ"
         return false;
      }

      return true ;
   }

   virtual void print() const {
      cout << "DOS Header" << endl;
      FieldMap::print();
   }
}; // DOS_FieldMap


/// A COFF-specific FieldMap
class COFF_FieldMap : public FieldMap {
public:
   /// Create a new COFF_FieldMAp at `new_file_offset`
   ///
   /// @param new_file_offset The offset into PEFile.buffer_ for this group of fields
   COFF_FieldMap( const size_t new_file_offset ) {
      file_offset_ = new_file_offset;

      this->insert( { "01_coff_signature",            make_unique<Field<uint32_t>>( 0x00, "coff_signature"          , 0                   ) } );
      this->insert( { "02_coff_machine",              make_unique<Field<uint16_t>>( 0x04, "Machine"                 , AS_HEX | WITH_FLAG  ) } );
      this->insert( { "03_coff_sections",             make_unique<Field<uint16_t>>( 0x06, "Number of Sections"      , AS_DEC              ) } );
      this->insert( { "04_coff_timedatestamp",        make_unique<Field<uint32_t>>( 0x08, "Date/time stamp"         , AS_DEC | WITH_TIME  ) } );
      this->insert( { "05_coff_PointerToSymbolTable", make_unique<Field<uint32_t>>( 0x0C, "Symbol Table offset"     , AS_DEC              ) } );
      this->insert( { "06_coff_NumberOfSymbols",      make_unique<Field<uint32_t>>( 0x10, "Number of symbols"       , AS_DEC              ) } );
      this->insert( { "07_coff_SizeOfOptionalHeader", make_unique<Field<uint16_t>>( 0x14, "Size of optional header" , AS_HEX              ) } );
      this->insert( { "08_coff_characteristics",      make_unique<Field<uint16_t>>( 0x16, "Characteristics"         , AS_HEX | WITH_FLAGS ) } );
   }

   /// @return The file offset to the top of the section table
   uint32_t get_section_table_offset() {
      // The first section starts immediately after the optional header...
      // So, it's at: file_offset_ + 18 (size of the COFF header) + coff_SizeOfOptionalHeader
      return file_offset_ + 0x18 + dynamic_cast<Field<uint16_t>&>( *this->at( "07_coff_SizeOfOptionalHeader" ) ).value_;
   }

   /// @return The number of sections in this PEFile
   uint16_t get_number_of_sections() {
      return dynamic_cast<Field<uint16_t>&>( *this->at( "03_coff_sections" ) ).value_;
   }

   virtual bool validate() const {
      if( !FieldMap::validate() ) {
         return false;
      }

      const uint16_t signature = dynamic_cast<Field<uint32_t>&>( *this->at( "01_coff_signature" ) ).value_;
      if( signature != 0x4550 ) { // Validate the magic is "PE"
         return false;
      }

      return true ;
   }

   virtual void print() const {
      cout << "COFF/File header" << endl;
      FieldMap::print();
   }
}; // COFF_FieldMap


/// A Section-specific FieldMap
class Section_FieldMap : public FieldMap {
public:
   /// Create a new Section_FieldMap at `new_file_offset`
   ///
   /// @param new_file_offset The offset into PEFile.buffer_ for this group of fields
   Section_FieldMap( const size_t new_file_offset ) {
      file_offset_ = new_file_offset;

      this->insert( { "01_section_name",                make_unique<Field<uint64_t>>( 0x00, "    Name"                 , AS_CHAR             ) } );
      this->insert( { "02_section_virtual_size",        make_unique<Field<uint32_t>>( 0x08, "    Virtual Size"         , AS_DEC | AS_HEX     ) } );
      this->insert( { "03_section_virtual_Address",     make_unique<Field<uint32_t>>( 0x0C, "    Virtual Address"      , AS_HEX              ) } );
      this->insert( { "04_section_raw_size",            make_unique<Field<uint32_t>>( 0x10, "    Size Of Raw Data"     , AS_DEC | AS_HEX     ) } );
      this->insert( { "05_section_raw_offset",          make_unique<Field<uint32_t>>( 0x14, "    Pointer To Raw Data"  , AS_HEX              ) } );
      this->insert( { "06_section_NumberOfRelocations", make_unique<Field<uint16_t>>( 0x20, "    Number Of Relocations", AS_HEX              ) } );
      this->insert( { "07_section_characteristics",     make_unique<Field<uint32_t>>( 0x24, "    Characteristics"      , AS_HEX | WITH_FLAGS ) } );
   }

   virtual void print() const {
      cout << "    Section" << endl;
      FieldMap::print();
   }
}; // Section_FieldMap


/// This class represents a Windows Portable Executable file
class PEFile {
protected:
   string       file_path_;  ///< The name of the PEFile
   long         file_size_;  ///< The size of the PEFile
   vector<char> buffer_;     ///< The contents of the PEFile

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

   /// Print the headers and sections of this PEFile
   virtual void print() {
      DOS_FieldMap dos_field_map_;

      dos_field_map_.parse( buffer_ );
      if( !dos_field_map_.validate() ) {
         cout << "The DOS header is invalid" << endl;
         exit( 1 );
      }
      dos_field_map_.print();

      const uint32_t coff_offset = dos_field_map_.get_exe_header_offset();

      COFF_FieldMap coff_header_map { coff_offset };
      coff_header_map.parse( buffer_ );
      if( !coff_header_map.validate() ) {
         cout << "The COFF header is invalid" << endl;
         exit( 1 );
      }
      coff_header_map.print();

      // std::vector<Section_FieldMap*> sections;
      cout << "Sections" << endl;

      for( size_t i = 0 ; i < coff_header_map.get_number_of_sections() ; i++ ) {
         Section_FieldMap newSection { coff_header_map.get_section_table_offset() + (i * 0x28) };
         newSection.parse( buffer_ );
         if( !newSection.validate() ) {
            cout << "A section header is invalid" << endl;
            exit( 1 );
         }
         newSection.print();
         //sections.push_back( newSection );
         cout << endl;
      }
   } // print()
}; // PEFile


/// Main entry point for readpe
/// @param argc The number of arguments
/// @param argv An array of arguments as strings
int main( int argc, char* argv[] ) {
   try {
      if( argc <= 1 ) {
         cout << "Usage:  readpe PEfile" << endl;
      }

      /// @todo Convert into a for() loop and process all of the files on the command line
      PEFile pe_file( argv[1] );
      pe_file.print();

      return 0;  /// @return The result code for this program
   } catch ( exception& catchAll ) {
      cout << "readpe threw an uncaught exception" << endl;
   }
} // main()
