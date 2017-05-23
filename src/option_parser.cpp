#include "option_parser.h"
#include <boost/program_options.hpp>

std::tuple<int, std::string, std::string> ParseOptions(int argc, char *argv[])
{
	using namespace boost::program_options;
	options_description options("Options:");
	options.add_options()
		("help,h", "Display this information")
		("input", value<std::vector<std::string>>()->value_name("file"), "Input file")
		("output,o", value<std::string>()->value_name("file"), "Place the output into <file>")
		("fdisable-access-protect", "Set the flag of disable access protect")
		("optim-reg-alloc", "Optimize the register allocation");

	positional_options_description po;
	po.add("input", 1);

	variables_map vm;

	try
	{
		store(command_line_parser(argc, argv).options(options).positional(po).run(), vm);
		notify(vm);
	}
	catch (std::exception &e)
	{
		std::cerr << e.what() << std::endl;
		return std::make_tuple(1, "", "");
	}

	if (vm.count("help"))
	{
		std::cout << "Usage: " << argv[0] << " [options] file" << std::endl;
		std::cout << options;
		return std::make_tuple(0, "", "");
	}
	if (vm.count("fdisable-access-protect"))
		CompileFlags::getInstance()->disable_access_protect = true;
	if (vm.count("optim-reg-alloc"))
		CompileFlags::getInstance()->optim_register_allocation = true;
	if (!vm.count("input"))
	{
		std::cerr << argv[0] << ": no input file" << std::endl;
		return std::make_tuple(1, "", "");
	}
	std::string input = vm["input"].as<std::vector<std::string>>()[0];
	std::string output = vm.count("output") ? vm["output"].as<std::string>() : "a.out";
	return std::make_tuple(0, input, output);
}