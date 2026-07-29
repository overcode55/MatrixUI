#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <iostream>
#include <fstream>
#include <tuple>
#include <variant>
#include <functional>
#include <cstdlib>
#include <filesystem>

#ifdef IMPLEMENT_MATRIX_UI
class MatrixUI{
  private:
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

  std::string get_file_type(const std::string& file){
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
    EOF_TOKEN,
    PROPERTY,
    PROPERTY_VALUE,
    EQUAL,
    TAG,
    BRAKET_OPEN_SLASH,
    BRAKET_CLOSE_SLASH,
    STRING
  };

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
    SEMICOLON,
    EOF_TOKEN
  };


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

    if ((c & 0x80) == 0) {
      return c;
    }
    
    if ((c & 0xE0) == 0xC0) {
      int code = c & 0x1F;
      code = (code << 6) | (static_cast<unsigned char>(buffer[pos++]) & 0x3F);
      if(peeking) pos--;
      return code;
    }
    
    if ((c & 0xF0) == 0xE0) {
      int code = c & 0x0F;
      code = (code << 6) | (static_cast<unsigned char>(buffer[pos++]) & 0x3F);
      code = (code << 6) | (static_cast<unsigned char>(buffer[pos++]) & 0x3F);
      if(peeking) pos-=2;
      return code;
    }
    
    if ((c & 0xF8) == 0xF0) {
      int code = c & 0x07;
      code = (code << 6) | (static_cast<unsigned char>(buffer[pos++]) & 0x3F);
      code = (code << 6) | (static_cast<unsigned char>(buffer[pos++]) & 0x3F);
      code = (code << 6) | (static_cast<unsigned char>(buffer[pos++]) & 0x3F);
      if(peeking) pos-=3;
      return code;
    }
    
    if(peeking) pos--;
    return -1;
  }

  std::vector<Basic_Token> unpack_buffer(std::string& buffer , std::string file){
    int c = 1;
    int pos = 0;
    size_t line = 1;
    size_t column = 0;

    auto next = [&](bool expect_eof = true){
      if(pos >= buffer.size()){
        if(!expect_eof){
          log_err(std::string("unexpected end of file, in file:") + file);
          return false;
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

    // FIXED: Cleaned parsing blocks inside lambda expression
    auto peek = [&](bool expect_eof = true){
      if(pos >= buffer.size()){
        if(!expect_eof){
          log_err(std::string("unexpected end of file, in file:") + file);
          return false;
        }else{
          return false;
        }
      }else{
        c = utf8_decode(buffer , pos , true);
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

    // shared low level helpers used by both the xml and the css lexers.
    // these only classify single characters, they don't know anything
    // about xml/css grammar, so they carry no "logic" of their own.
    auto is_alpha = [](unsigned char ch){
      return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_' || ch >= 128;
    };
    auto is_digit = [](unsigned char ch){
      return ch >= '0' && ch <= '9';
    };
    auto is_ident_char = [&](unsigned char ch){
      return is_alpha(ch) || is_digit(ch) || ch == '-';
    };
    auto is_space = [](unsigned char ch){
      return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
    };
    auto raw_peek = [&](size_t offset = 0) -> char {
      size_t p = static_cast<size_t>(pos) + offset;
      return p < buffer.size() ? buffer[p] : '\0';
    };
    auto advance_raw = [&](size_t count = 1){
      for(size_t i = 0; i < count && static_cast<size_t>(pos) < buffer.size(); i++){
        if(buffer[pos] == '\n'){ line++; column = 0; }
        else column++;
        pos++;
      }
    };

    if(file_type == ".xml"){
      // xml here follows plain html-like markup (tags + attributes),
      // just without doctype/head - a single <root>...</root> is the
      // only thing required, but that's enforced later, not by the lexer.

      auto read_ident = [&]() -> std::string {
        std::string ident;
        while(static_cast<size_t>(pos) < buffer.size() && is_ident_char(static_cast<unsigned char>(buffer[pos]))){
          ident += buffer[pos];
          advance_raw();
        }
        return ident;
      };

      auto skip_ws = [&](){
        while(static_cast<size_t>(pos) < buffer.size() && is_space(static_cast<unsigned char>(buffer[pos]))){
          advance_raw();
        }
      };

      auto read_quoted = [&](char quote) -> std::string {
        std::string value;
        while(static_cast<size_t>(pos) < buffer.size() && buffer[pos] != quote){
          value += buffer[pos];
          advance_raw();
        }
        if(static_cast<size_t>(pos) < buffer.size()) advance_raw(); // closing quote
        else log_err(std::string("unterminated string literal in file: ") + file);
        return value;
      };

      bool inside_tag = false;

      while(static_cast<size_t>(pos) < buffer.size()){
        skip_ws();
        if(static_cast<size_t>(pos) >= buffer.size()) break;

        char ch = buffer[pos];

        if(!inside_tag){
          if(ch == '<'){
            if(raw_peek(1) == '/'){
              advance_raw(2);
              Basic_Token_Type t; t.xml_type = XML_Basic_Token_Type::BRAKET_OPEN_SLASH;
              std::string raw = "</";
              push(t, raw);

              skip_ws();
              std::string tag = read_ident();
              if(tag.empty()){
                log_err(std::string("expected tag name after </ in file: ") + file);
              } else {
                Basic_Token_Type tt; tt.xml_type = XML_Basic_Token_Type::TAG;
                push(tt, tag);
              }

              skip_ws();
              if(static_cast<size_t>(pos) < buffer.size() && buffer[pos] == '>'){
                advance_raw();
                Basic_Token_Type ct; ct.xml_type = XML_Basic_Token_Type::BRAKET_CLOSE;
                std::string raw2 = ">";
                push(ct, raw2);
              } else {
                log_err(std::string("expected '>' to close tag in file: ") + file);
              }
            } else {
              advance_raw();
              Basic_Token_Type t; t.xml_type = XML_Basic_Token_Type::BRAKET_OPEN;
              std::string raw = "<";
              push(t, raw);

              skip_ws();
              std::string tag = read_ident();
              if(tag.empty()){
                log_err(std::string("expected tag name after < in file: ") + file);
              } else {
                Basic_Token_Type tt; tt.xml_type = XML_Basic_Token_Type::TAG;
                push(tt, tag);
              }
              inside_tag = true;
            }
          } else {
            // free text between tags - tokenized as STRING
            std::string text;
            while(static_cast<size_t>(pos) < buffer.size() && buffer[pos] != '<'){
              text += buffer[pos];
              advance_raw();
            }
            // skip pure-whitespace runs (formatting indentation), keep real content
            bool only_ws = true;
            for(char tc : text){
              if(!is_space(static_cast<unsigned char>(tc))){ only_ws = false; break; }
            }
            if(!only_ws){
              Basic_Token_Type t; t.xml_type = XML_Basic_Token_Type::STRING;
              push(t, text);
            }
          }
        } else {
          if(ch == '/' && raw_peek(1) == '>'){
            advance_raw(2);
            Basic_Token_Type t; t.xml_type = XML_Basic_Token_Type::BRAKET_CLOSE_SLASH;
            std::string raw = "/>";
            push(t, raw);
            inside_tag = false;
          }
          else if(ch == '>'){
            advance_raw();
            Basic_Token_Type t; t.xml_type = XML_Basic_Token_Type::BRAKET_CLOSE;
            std::string raw = ">";
            push(t, raw);
            inside_tag = false;
          }
          else if(is_alpha(static_cast<unsigned char>(ch))){
            std::string prop = read_ident();
            Basic_Token_Type t; t.xml_type = XML_Basic_Token_Type::PROPERTY;
            push(t, prop);

            skip_ws();
            if(static_cast<size_t>(pos) < buffer.size() && buffer[pos] == '='){
              advance_raw();
              Basic_Token_Type et; et.xml_type = XML_Basic_Token_Type::EQUAL;
              std::string raw = "=";
              push(et, raw);

              skip_ws();
              if(static_cast<size_t>(pos) < buffer.size() && (buffer[pos] == '"' || buffer[pos] == '\'')){
                char quote = buffer[pos];
                advance_raw();
                std::string value = read_quoted(quote);
                Basic_Token_Type vt; vt.xml_type = XML_Basic_Token_Type::PROPERTY_VALUE;
                push(vt, value);
              } else {
                log_err(std::string("expected quoted string for property value in file: ") + file);
              }
            }
            // a property with no '=' is a valueless/boolean attribute
          }
          else {
            log_err(std::string("unexpected character inside tag in file: ") + file);
            advance_raw();
          }
        }
      }

      Basic_Token_Type eof_t; eof_t.xml_type = XML_Basic_Token_Type::EOF_TOKEN;
      std::string eof_raw = "";
      push(eof_t, eof_raw);
    }
    else if(file_type == ".css"){
      // normal css structure: selectors, declaration blocks, functions,
      // attribute selectors and combinators.

      auto skip_ws_comments = [&](){
        while(static_cast<size_t>(pos) < buffer.size()){
          char ch = buffer[pos];
          if(is_space(static_cast<unsigned char>(ch))){
            advance_raw();
          } else if(ch == '/' && raw_peek(1) == '*'){
            advance_raw(2);
            while(static_cast<size_t>(pos) < buffer.size() && !(buffer[pos] == '*' && raw_peek(1) == '/')){
              advance_raw();
            }
            if(static_cast<size_t>(pos) < buffer.size()) advance_raw(2);
          } else {
            break;
          }
        }
      };

      auto read_ident = [&]() -> std::string {
        std::string ident;
        while(static_cast<size_t>(pos) < buffer.size() && is_ident_char(static_cast<unsigned char>(buffer[pos]))){
          ident += buffer[pos];
          advance_raw();
        }
        return ident;
      };

      auto read_quoted = [&](char quote) -> std::string {
        std::string value;
        while(static_cast<size_t>(pos) < buffer.size() && buffer[pos] != quote){
          value += buffer[pos];
          advance_raw();
        }
        if(static_cast<size_t>(pos) < buffer.size()) advance_raw(); // closing quote
        else log_err(std::string("unterminated string literal in file: ") + file);
        return value;
      };

      // reads an identifier and, if present, a trailing decimal part and/or
      // a trailing '%' - used for value-ish contexts like "1.5em" or "50%"
      auto read_ident_with_suffix = [&]() -> std::string {
        std::string ident = read_ident();
        if(static_cast<size_t>(pos) < buffer.size() && buffer[pos] == '.' && is_digit(static_cast<unsigned char>(raw_peek(1)))){
          ident += buffer[pos];
          advance_raw();
          ident += read_ident();
        }
        if(static_cast<size_t>(pos) < buffer.size() && buffer[pos] == '%'){
          ident += buffer[pos];
          advance_raw();
        }
        return ident;
      };

      auto read_value_token = [&]() -> std::string {
        if(static_cast<size_t>(pos) < buffer.size() && (buffer[pos] == '"' || buffer[pos] == '\'')){
          char quote = buffer[pos];
          advance_raw();
          return read_quoted(quote);
        }
        return read_ident_with_suffix();
      };

      // tokenizes the argument list of a function call, up to and including
      // its matching closing ')'. handles nested function calls (e.g. var()
      // inside calc()) by recursing on themselves.
      std::function<void()> tokenize_call_args = [&](){
        while(static_cast<size_t>(pos) < buffer.size()){
          skip_ws_comments();
          if(static_cast<size_t>(pos) >= buffer.size()) break;
          char fc = buffer[pos];

          if(fc == ')'){
            advance_raw();
            return;
          }
          if(fc == ','){
            // argument separator - no dedicated token type at this level
            advance_raw();
            continue;
          }
          if(fc == '+' || fc == '-' || fc == '*' || fc == '/'){
            advance_raw();
            Basic_Token_Type mt; mt.css_type = CSS_Basic_Token_Type::MATH;
            std::string raw = std::string(1 , fc);
            push(mt, raw);
            continue;
          }
          if(fc == '"' || fc == '\''){
            advance_raw();
            std::string value = read_quoted(fc);
            Basic_Token_Type vt; vt.css_type = CSS_Basic_Token_Type::FUNCTION_VALUE;
            push(vt, value);
            continue;
          }
          if(is_alpha(static_cast<unsigned char>(fc)) || is_digit(static_cast<unsigned char>(fc))){
            std::string term = read_ident_with_suffix();
            if(static_cast<size_t>(pos) < buffer.size() && buffer[pos] == '('){
              advance_raw();
              Basic_Token_Type ft; ft.css_type = CSS_Basic_Token_Type::FUNCTION_NAME;
              push(ft, term);
              tokenize_call_args();
            } else {
              Basic_Token_Type vt; vt.css_type = CSS_Basic_Token_Type::FUNCTION_VALUE;
              push(vt, term);
            }
            continue;
          }

          // anything else inside a call is skipped at the basic-token level
          advance_raw();
        }
      };

      int brace_depth = 0;

      while(true){
        skip_ws_comments();
        if(static_cast<size_t>(pos) >= buffer.size()) break;

        char ch = buffer[pos];
        bool in_block = brace_depth > 0;

        if(ch == '{'){
          advance_raw();
          brace_depth++;
          Basic_Token_Type t; t.css_type = CSS_Basic_Token_Type::BRAKET_OPEN;
          std::string raw = "{";
          push(t, raw);
        }
        else if(ch == '}'){
          advance_raw();
          if(brace_depth > 0) brace_depth--;
          Basic_Token_Type t; t.css_type = CSS_Basic_Token_Type::BRAKET_CLOSE;
          std::string raw = "}";
          push(t, raw);
        }
        else if(ch == ';'){
          advance_raw();
          Basic_Token_Type t; t.css_type = CSS_Basic_Token_Type::SEMICOLON;
          std::string raw = ";";
          push(t, raw);
        }
        else if(ch == ','){
          // still no dedicated token type for argument/selector-list
          // separators, so this one is consumed as before
          advance_raw();
        }
        else if(ch == '.'){
          advance_raw();
          std::string name = read_ident();
          Basic_Token_Type t; t.css_type = CSS_Basic_Token_Type::CLASS;
          push(t, name);
        }
        else if(ch == '#'){
          advance_raw();
          std::string name = read_ident();
          Basic_Token_Type t; t.css_type = CSS_Basic_Token_Type::ID;
          push(t, name);
        }
        else if(ch == '&'){
          advance_raw();
          Basic_Token_Type t; t.css_type = CSS_Basic_Token_Type::ANDPERCENT;
          std::string raw = "&";
          push(t, raw);
        }
        else if(ch == '>'){
          advance_raw();
          Basic_Token_Type t; t.css_type = CSS_Basic_Token_Type::CHILD_SELECTOR;
          std::string raw = ">";
          push(t, raw);
        }
        else if(ch == '~'){
          advance_raw();
          Basic_Token_Type t; t.css_type = CSS_Basic_Token_Type::GENERAL_SIBLING_SELECTOR;
          std::string raw = "~";
          push(t, raw);
        }
        else if(ch == '+'){
          advance_raw();
          Basic_Token_Type t;
          t.css_type = in_block ? CSS_Basic_Token_Type::MATH : CSS_Basic_Token_Type::ADJACENT_SELECTOR;
          std::string raw = "+";
          push(t, raw);
        }
        else if(ch == '-' || ch == '/'){
          advance_raw();
          Basic_Token_Type t; t.css_type = CSS_Basic_Token_Type::MATH;
          std::string raw = std::string(1 , ch);
          push(t, raw);
        }
        else if(ch == '*'){
          advance_raw();
          Basic_Token_Type t;
          t.css_type = in_block ? CSS_Basic_Token_Type::MATH : CSS_Basic_Token_Type::UNIVERSAL;
          std::string raw = "*";
          push(t, raw);
        }
        else if(ch == ':'){
          if(raw_peek(1) == ':'){
            advance_raw(2);
            Basic_Token_Type t; t.css_type = CSS_Basic_Token_Type::DOUBLE_COLON;
            std::string raw = "::";
            push(t, raw);
          } else {
            advance_raw();
            Basic_Token_Type t; t.css_type = CSS_Basic_Token_Type::COLON;
            std::string raw = ":";
            push(t, raw);
          }
        }
        else if(ch == '['){
          advance_raw();
          skip_ws_comments();
          std::string attr_name = read_ident();
          if(attr_name.empty()){
            log_err(std::string("expected attribute name inside [] in file: ") + file);
          }
          skip_ws_comments();

          if(static_cast<size_t>(pos) < buffer.size() && buffer[pos] == ']'){
            advance_raw();
            Basic_Token_Type t; t.css_type = CSS_Basic_Token_Type::ATRIBUTE_EXISTS;
            push(t, attr_name);
          }
          else if(static_cast<size_t>(pos) < buffer.size() && buffer[pos] == '='){
            advance_raw();
            Basic_Token_Type t; t.css_type = CSS_Basic_Token_Type::ATRIBUTE_SLECTOR;
            push(t, attr_name);
            skip_ws_comments();
            std::string value = read_value_token();
            Basic_Token_Type vt; vt.css_type = CSS_Basic_Token_Type::ATRIBUTE_SLECTOR_VALUE;
            push(vt, value);
            skip_ws_comments();
            if(static_cast<size_t>(pos) < buffer.size() && buffer[pos] == ']') advance_raw();
          }
          else if(static_cast<size_t>(pos) < buffer.size() && buffer[pos] == '^' && raw_peek(1) == '='){
            advance_raw(2);
            Basic_Token_Type t; t.css_type = CSS_Basic_Token_Type::ATRIBUTE_STARTS_WITH;
            push(t, attr_name);
            skip_ws_comments();
            std::string value = read_value_token();
            Basic_Token_Type vt; vt.css_type = CSS_Basic_Token_Type::ATRIBUTE_STARTS_WITH_VALUE;
            push(vt, value);
            skip_ws_comments();
            if(static_cast<size_t>(pos) < buffer.size() && buffer[pos] == ']') advance_raw();
          }
          else if(static_cast<size_t>(pos) < buffer.size() && buffer[pos] == '$' && raw_peek(1) == '='){
            advance_raw(2);
            Basic_Token_Type t; t.css_type = CSS_Basic_Token_Type::ATRIBUTE_ENDS_WITH;
            push(t, attr_name);
            skip_ws_comments();
            std::string value = read_value_token();
            Basic_Token_Type vt; vt.css_type = CSS_Basic_Token_Type::ATRIBUTE_ENDS_WITH_VALUE;
            push(vt, value);
            skip_ws_comments();
            if(static_cast<size_t>(pos) < buffer.size() && buffer[pos] == ']') advance_raw();
          }
          else if(static_cast<size_t>(pos) < buffer.size() && buffer[pos] == '*' && raw_peek(1) == '='){
            advance_raw(2);
            Basic_Token_Type t; t.css_type = CSS_Basic_Token_Type::ATRIBUTE_CONTAINS_WITH;
            push(t, attr_name);
            skip_ws_comments();
            std::string value = read_value_token();
            Basic_Token_Type vt; vt.css_type = CSS_Basic_Token_Type::ATRIBUTE_CONTAINS_WITH_VALUE;
            push(vt, value);
            skip_ws_comments();
            if(static_cast<size_t>(pos) < buffer.size() && buffer[pos] == ']') advance_raw();
          }
          else {
            log_err(std::string("malformed attribute selector in file: ") + file);
            if(static_cast<size_t>(pos) < buffer.size() && buffer[pos] == ']') advance_raw();
          }
        }
        else if(ch == '"' || ch == '\''){
          char quote = ch;
          advance_raw();
          std::string value = read_quoted(quote);
          Basic_Token_Type t; t.css_type = CSS_Basic_Token_Type::TAG;
          push(t, value);
        }
        else if(is_alpha(static_cast<unsigned char>(ch)) || is_digit(static_cast<unsigned char>(ch))){
          std::string ident = read_ident_with_suffix();
          if(static_cast<size_t>(pos) < buffer.size() && buffer[pos] == '('){
            advance_raw();
            Basic_Token_Type t; t.css_type = CSS_Basic_Token_Type::FUNCTION_NAME;
            push(t, ident);
            tokenize_call_args();
          } else {
            Basic_Token_Type t; t.css_type = CSS_Basic_Token_Type::TAG;
            push(t, ident);
          }
        }
        else {
          log_err(std::string("unexpected character in css file: ") + file);
          advance_raw();
        }
      }

      Basic_Token_Type eof_t; eof_t.css_type = CSS_Basic_Token_Type::EOF_TOKEN;
      std::string eof_raw = "";
      push(eof_t, eof_raw);
    }
    else if(file_type == ".lua"){
      // logic placeholder
    }
    return BTs;
  }

  void get_basic_tokens(std::unordered_map<std::string , std::string>& buffers , std::unordered_map<std::string , std::vector<Basic_Token>>& Basic_Tokens){
    for(auto& [file , buffer] : buffers){
      Basic_Tokens[file] = unpack_buffer(buffer , file);
    }
  }

  std::string compile_xml(std::unordered_map<std::string , std::string>& xml_buffers , Context& context , std::ofstream& output){
    std::unordered_map<std::string , std::vector<Basic_Token>> Basic_Tokens;
    get_basic_tokens(xml_buffers , Basic_Tokens);
    return "";
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
    return 0;
  }

  public:

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

    return transcompile(context);
  }

};

#else

class MatrixUI{
  public:
  int compile(const std::vector<std::string>& argv);
};

#endif
