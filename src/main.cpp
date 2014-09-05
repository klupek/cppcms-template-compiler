#include "parser.h"
#include <sstream>
#include <iostream>
#include <fstream>

void usage(const std::string& self) {
	std::cerr << self << " [--code(default) | --ast | --parse ] [ -s SKIN NAME ] file1.tmpl file2.tmpl ...\n";
	exit(1);
}

int main(int argc, char **argv) {
	std::ofstream out_file;
	std::ostream* out = &std::cout;
	std::vector<std::string> files;
	cppcms::templates::generator::context ctx;
	ctx.variable_prefix = "content."; // TODO: load defaults
	enum { code, ast, parse } mode = code;
	bool end_of_options = false;
	for(int i=1;i<argc;++i) {
		const std::string v(argv[i]);
		if(v == "--code") {
			mode = code;
		} else if(v == "--ast") {
			mode = ast;
		} else if(v == "--parse") {
			mode = parse;
		} else if(v == "-s") {
			if(i == argc-1) {
				usage(argv[0]);
			} else {
				ctx.skin = argv[i+1];
				++i;
			}
		} else if(v == "--" ){
			end_of_options = true;
		} else if(v == "-o" && i + 1 != argc) {			
			out = &out_file;
			out_file.open(argv[i+1]);
			if(!out_file) {
				std::cerr << "ERROR: could not open " << argv[i+1] << " for writing\n";
				usage(argv[0]);
			}
			i++;		
		} else if(v[0] == '-') {
			usage(argv[0]);
		} else {
			files.emplace_back(argv[i]);
		}
	}

	if(files.empty())
		usage(argv[0]);
	
	try {
		cppcms::templates::template_parser p(files);
		p.parse();
		if(mode == ast) {
			p.tree()->dump(*out);
		} else if(mode == code) {
			p.write(ctx, *out);
		} else {
			std::ostringstream oss;
			p.write(ctx, oss);
			std::cout << "parse: ok\n";
		}
	} catch(const std::logic_error& e) {
		std::cerr << "logic error(bug): " << e.what() << std::endl;
		return 2;
	} catch(const std::runtime_error& e) {
		std::cerr << e.what() << std::endl;
		return 3;
	}

	return 0;
}
