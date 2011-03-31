####
#
# Pylet - Python Language Engineering Toolkit
# Written by Sander Canisius <S.V.M.Canisius@uvt.nl>
#

import optparse
import warnings

from itertools import imap


def parseOption(lines, options, optionRefinements):
	line = lines.pop(0)
	switches, helpMessage = map(str.strip, line.split(":", 1))

	metavar = None
	opts = []
	for switch in imap(str.strip, switches.split(",")):
		if switch.startswith("--"):
			if "=" in switch:
				switch, metavar = switch.split("=")
			opts.append(switch)
			
		elif switch.startswith("-"):
			if len(switch) > 2:
				metavar = switch[2:].strip()
				switch = switch[:2]
			opts.append(switch)

		else:
			warnings.warn("Ignoring invalid option specification " + switch,
						  SyntaxWarning, stacklevel=3)

	stop = False
	while lines and not stop:
		nextLine = lines[0]
		text = nextLine.strip()
		if text and nextLine[0].isspace():
			helpMessage += " " + text
			lines.pop(0)
		else:
			stop = True
	
	kwargs = {
		"help": helpMessage
	}

	if metavar:
		kwargs["metavar"] = metavar
	else:
		kwargs["action"] = "store_true"

	if opts[0] in optionRefinements:
		kwargs.update(optionRefinements[opts[0]])

	options.add_option(*opts, **kwargs)


def parseOptionGroup(lines, parser):
	line = lines.pop(0)
	desc = line[:-1].strip()
	return parser.add_option_group(desc)


def parseRefinements(spec):
	lines = spec.splitlines()[2:]   # remove ---\n:Refinements:
	
	refinements = {}
	for line in lines:
		option, attributes = imap(str.strip, line.split(":", 1))
		refinements[option] = eval("dict(%s)" % attributes)

	return refinements


def parse(optionSpec=None, args=None):
	import __main__
	if not optionSpec:
		optionSpec = __main__.__doc__
	
	start = optionSpec.index("usage:")
	try:
		end = optionSpec.index("---\n:Refinements:")
		refinements = parseRefinements(optionSpec[end:]) # NOTE: this should be in its own try block!?
	except:
		end = len(optionSpec)
		refinements = {}

	description = optionSpec[:start].strip()

	lines = optionSpec[start:end].splitlines()
	
	usage = lines.pop(0)[6:].strip()
	parser = optparse.OptionParser(usage=usage,
								   description=description,
								   version=getattr(__main__,
												   "__version__", None))
	options = parser

	while lines:
		nextLine = lines[0]
		if nextLine.startswith("-"):
			parseOption(lines, options, refinements)
		elif nextLine.endswith(":"):
			options = parseOptionGroup(lines, parser)
		else:
			lines.pop(0)

	return parser.parse_args(args)
