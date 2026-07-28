#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <iostream>
#include <fstream>
#include <tuple>
#include <variant>
#include <cstdlib>
#include <filesystem>


#ifndef IMPLEMENT_MATRIX_UI 

struct Context{
  std::unordered_map<std::string , std::string> xml_buffers;
  std::unordered_map<std::string , std::string> css_buffers;
  std::unordered_map<std::string , std::string> lua_buffers;
  std::string output_dir;
  std::string output_name;
};

void log(std::string_view str){
  std::cout << str << "\n";
}
void log_err(std::string_view str){
  std::cerr << "\033[38;2;255;90;90m" << str << "\033[0m" << "\n";
}

constexpr unsigned int hash_string(std::string_view str) {
    unsigned int hash = 2166136261u;
    for (char c : str) {
        hash ^= static_cast<unsigned int>(c);
        hash *= 16777619u;
    }
    return hash;
}

void read_into_buffer(const std::string& path , std::string& buffer){
  std::ifstream file(path , std::ios::binary | std::ios::ate);
  if(!file.is_open()){
    log_err("couldn't open file");
    return;
  }

  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  buffer.resize(size);
  if (!file.read(buffer.data(), size)) {
    log_err(std::string("couldn't read file: ") + path);   
  }
}

std::vector<std::string> separate_files(std::string value){
  auto pos = value.find_first_of('*');
  std::vector<std::string> files{};
  while(pos != std::string::npos){
    std::string file = value.substr(0 , pos);
    files.push_back(file);
    value = value.substr(pos + 1 , value.size());
  }
  if (!value.empty()) {
    files.push_back(value);
  }
  return files;
}

std::string get_file_type(std::string& file){
  auto file_type_dot_pos = file.find_last_of('.');
  if(file_type_dot_pos == std::string::npos){
    log_err(std::string("expected file type for: ") + file);
    return "";
  }
  return file.substr(file_type_dot_pos , file.size());
}

void read_file(const std::string& file , Context& context){
  std::string file_type = get_file_type(file);
       if(file_type == ".lua") read_into_buffer(file , context.lua_buffers[file]);
  else if(file_type == ".css") read_into_buffer(file , context.css_buffers[file]);
  else if(file_type == ".xml") read_into_buffer(file , context.xml_buffers[file]);
  else {
    log_err(std::string("unsuported file type: ") + file_type + std::string(" , from file: ") + file);
    exit(-1);
  }
}

enum class Argument_Types{
  OUTDIR,
  OUTNAME,
  FILE,
  UNKNOW
};

std::pair<Argument_Types , std::string> arg_type(const std::string& arg){
  auto argtype_end_pos = arg.find_first_of("=");

  if(argtype_end_pos == std::string::npos) return std::pair<Argument_Types , std::string>{Argument_Types::FILE , arg};

  auto argtype = arg.substr(0 , argtype_end_pos);
  std::string argvalue = arg.substr(argtype_end_pos + 1 , arg.size());
  if(argtype == "-outdir") return std::pair<Argument_Types , std::string>{Argument_Types::OUTDIR , argvalue};
  if(argtype == "-outname") return std::pair<Argument_Types , std::string>{Argument_Types::OUTNAME , argvalue};
  return std::pair<Argument_Types , std::string>{Argument_Types::UNKNOW , argvalue};
}

void figure_what_to_do(const std::string& arg , Context& context){
  auto [type , value] = arg_type(arg);

  switch (type)
  {
  case Argument_Types::OUTDIR:
    context.output_dir = value;
    break;
  case Argument_Types::OUTNAME:
    context.output_name = value;
    break;
  case Argument_Types::FILE:
    read_file(arg , context);
    break;
  default:
    log_err(std::string("unknow argument: ") + arg);
    break;
  }
}

enum class XML_Basic_Token_Type{
  BRAKET_OPEN,
  BRAKET_CLOSE,
  EOF,
  PROPERTY,
  PROPERTY_VALUE,
  EQUAL,
  TAG,
  BRAKET_OPEN_SLASH,
  BRAKET_CLOSE_SLASH,
  STRING
}
enum class CSS_Basic_Token_Type{
  ANDPERCENT,
  BRAKET_OPEN,
  BRAKET_CLOSE,
  FUNCTION_NAME,
  FUNCTION_VALUE,
  DOUBLE_COLON,
  COLON,
  TAG,
  CLASS,
  ID,
  UNIVERSAL,
  CHILD_SELECTOR,
  ADJACENT_SELECTOR,
  GENERAL_SIBLING_SELECTOR,
  ATRIBUTE_SLECTOR,
  ATRIBUTE_SLECTOR_VALUE,
  ATRIBUTE_EXISTS,
  ATRIBUTE_STARTS_WITH,
  ATRIBUTE_STARTS_WITH_VALUE,
  ATRIBUTE_ENDS_WITH,
  ATRIBUTE_ENDS_WITH_VALUE,
  ATRIBUTE_CONTAINS_WITH,
  ATRIBUTE_CONTAINS_WITH_VALUE,
  MATH,
  EOF
}


union Basic_Token_Type{
  XML_Basic_Token_Type xml_type;
  CSS_Basic_Token_Type css_type;
};

struct Basic_Token{
  Basic_Token_Type type;
  std::string raw_value;
  size_t line,column;
  std::string file;
};

int utf8_decode(std::string& buffer , int& pos , bool peeking = false){
  unsigned char c = buffer[pos++];
  if(peeking) pos--;

  // 1-byte ASCII (0xxxxxxx)
  if ((c & 0x80) == 0) {
    return c;
  }
  
  // 2-byte sequence (110xxxxx)
  if ((c & 0xE0) == 0xC0) {
    int code = c & 0x1F;
    code = (code << 6) | (static_cast<unsigned char>(buffer[pos++]) & 0x3F);
    if(peeking) pos--;
    return code;
  }
  
  // 3-byte sequence (1110xxxx)
  if ((c & 0xF0) == 0xE0) {
    int code = c & 0x0F;
    code = (code << 6) | (static_cast<unsigned char>(buffer[pos++]) & 0x3F);
    code = (code << 6) | (static_cast<unsigned char>(buffer[pos++]) & 0x3F);
    if(peeking) pos-=2;
    return code;
  }
  
  // 4-byte sequence (11110xxx)
  if ((c & 0xF8) == 0xF0) {
    int code = c & 0x07;
    code = (code << 6) | (static_cast<unsigned char>(buffer[pos++]) & 0x3F);
    code = (code << 6) | (static_cast<unsigned char>(buffer[pos++]) & 0x3F);
    code = (code << 6) | (static_cast<unsigned char>(buffer[pos++]) & 0x3F);
    if(peeking) pos-=3;
    return code;
  }
  
  if(peeking) pos--;

  return -1; // Invalid UTF-8 leading byte
}

std::vector<Basic_Token> unpack_buffer(std::string& buffer , std::string file){
  int c = 1;
  int pos = 0;
  size_t line , column = 0;
  auto next = [&](bool expect_eof = true){
    if(pos >= buffer.size()){
      if(!expect_eof){
        log_err(std::string("unexpected end of file, in file:") + file);
      }else{
        return false;
      }
    }else{
      c = utf8_decode(buffer , pos);
      if(c == '\n'){
        line++;
        column = 0;
      }else{
        column++;
      }
    }
    return true;
  };
  auto peek = [&](){
        if(pos >= buffer.size()){
      if(!expect_eof){
        log_err(std::string("unexpected end of file, in file:") + file);
      }else{
        return false;
      }
    }else{
      c = utf8_decode(buffer , pos , true);
      }
    }
    return true;
  };

  std::vector<Basic_Token> BTs{};

  auto push = [&](Basic_Token_Type type , std::string& raw_value){
    Basic_Token token{
      .type = type,
      .raw_value = raw_value,
      .line = line,
      .column = column,
      .file = file
    };
    BTs.push_back(token);
  };
  std::string file_type = get_file_type(file);
  if(file_type == ".xml"){
    while(next()){
      if(c == '<'){
        push(XML_Basic_Token_Type::BRAKET_OPEN , "");
      }
      else if(c == '>')
    } 
  }
  else if(file_type == ".css"){ 
    while(next()){
      if()
    }
  }
  else if(file_type == ".lua"){

  }
}

void get_basic_tokens(std::unordered_map<std::string , std::string>& buffers , std::unordered_map<std::string , std::vector<Basic_Token>>& Basic_Tokens){
  for(auto& [file , buffer] : buffers){
    Basic_Tokens[file] = unpack_buffer(buffer , file);
  }
}

std::string compile_xml(std::unordered_map<std::string , std::string>& xml_buffers , Context& context , std::ofstream& output){
  std::unordered_map<std::string , std::vector<Basic_Token>> Basic_Tokens;
  get_basic_tokens(xml_buffers , Basic_Tokens);
  
}

int transcompile(Context& context){
  std::filesystem::path output_path = context.output_dir + context.output_name;
  std::ofstream output(output_path);
  bool success = false;
  if(!output.is_open()){
    try {
      if (std::filesystem::create_directory(context.output_dir)) {
          success = true;
      } else {
          log_err("Folder already exists or could not be created");
      }
    } catch (const std::filesystem::filesystem_error& e) {
        log_err(std::string("Filesystem error: ") + std::string(e.what()));
    }
    if(!success) return -1;
    else output.open(output_path);
  }
  if(!output.is_open()){
    log_err("output directory is wrong or other errors took place while trying to create directory!"); 
    return -1;
  }else{
    std::string compiled_xml = compile_xml(context.xml_buffers , context , output);
  }
}

int compile(const std::vector<std::string>& argv){

  if(argv.empty()){
    std::cerr << "expected argument(s)!\n";
    return -1;
  }

  Context context;

  for(auto arg : argv){
    figure_what_to_do(arg , context);
  } 

  if(context.xml_buffers.empty()){
    log_err("at least one xml file must be compiled!");
    return -1;
  }

  return transcompile(context);;
}
#endif