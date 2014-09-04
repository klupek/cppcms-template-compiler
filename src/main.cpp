#include "parser.h"
#include <sstream>
#include <iostream>

void usage(const std::string& self) {
	std::cerr << self << " [--code(default) | --ast | --parse ] [ -s SKIN NAME ] file1.tmpl file2.tmpl ...\n";
	exit(1);
}

int main(int argc, char **argv) {

	std::vector<std::string> files;
	cppcms::templates::generator::context ctx;
	ctx.variable_prefix = "content."; // TODO: load defaults
	enum { code, ast, parse } mode = code;

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
			p.tree()->dump(std::cout);
		} else if(mode == code) {
			p.write(ctx, std::cout);
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
