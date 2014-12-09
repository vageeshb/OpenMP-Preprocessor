/*
 * main.cc
 *  Created on: Oct 22, 2014
 *      Author: vageesh
 */

using namespace std;
#include "iostream"
#include "fstream"
#include "sstream"
#include "string"
#include "pthread.h"
#include "vector"
#include "time.h"

// Global variable declarations
string outputFileName = "";
string total_threads = "0";
string headers = "";
string global_definitions = "";
string local_definitions = "";
string main_body = "";
string thread_creation = "";
string for_loop_body = "";
string par_func_body = "";
string critical_block = "";
string single_block = "";
string thread_vars = "";
string barrier_body = "";
string extra_functions = "";
string section_block = "";
bool hasForLoop = false;
bool hasSections = false;
vector<string> shared_vars;
vector<string> local_vars;

/**
 * Utility function that removes extra spaces from a string
 */
string removeSpaces(string input) {
	for (unsigned i = 0; i < input.length(); i++) {
		if(input[i] == ' ')
			input.erase(i, 1);
	}
	return input;
}

/**
 * This method locates and assigns headers
 */
bool assignHeader(string fileName) {
	ifstream iFile(fileName);
	string line;
	bool status = false;

	if(iFile.is_open()) {
		while (getline(iFile, line)) {
			if (line.find("#include") != string::npos) {
				// Replacing omp header file with pthread header file
				if(line.find("omp.h") != string::npos) {
					headers 	+= "#include\t<pthread.h>\n";
				}
				else {
					headers 	+= line + "\n";
				}
				status = true;
			}
		}
	}
	iFile.close();
	return status;
}

/**
 * This method find the max number of threads to be
 * created in the program
 */
bool assignTotalThreads(string fileName) {
	ifstream iFile(fileName);
	string line;
	bool status = false;
	if(iFile.is_open()) {
		while(getline(iFile, line)) {
			// Looking for num_threads directive to extract
			// max parallel threads
			if(line.find("num_threads") != string::npos) {
				string part = line.erase(0, line.find("num_threads") + 12);
				total_threads = part.substr(0, part.find(")"));
				status = true;
				break;
			}
		}
	}
	iFile.close();
	return status;
}

/**
 * Utility function that adds extracted variables
 * to the global variable vector
 */
void addToDefs(string where, string line, string type, string initialValue) {
	vector<string> local;
	string item, lItem, tempItem;
	line.erase(0, line.find(type + " ") + type.length() + 1);
	line.erase(line.find(";"), line.find(";") + 1);
	line = removeSpaces(line);
	while(line.find(",") != string::npos) {
		local.push_back(line.substr(0, line.find(",")));
		line.erase(0, line.find(",") + 1);
	}
	local.push_back(line);

	// Add to global definitions
	if(where == "global") {
		for(unsigned i = 0; i < shared_vars.size(); i++) {
			item = shared_vars.at(i);
			for(unsigned j = 0; j < local.size(); j++) {
				lItem = local.at(j);
				tempItem = lItem;
				if(lItem.find("[") != string::npos)
					tempItem = lItem.substr(0, lItem.find("["));
				if(item.compare(tempItem) == 0)
					global_definitions += type + " " + lItem + ";\n";
			}
		}
	}
	// Add to local definitions
	else if(where == "local") {
		for(unsigned i = 0; i < local_vars.size(); i++) {
			item = local_vars.at(i);
			for(unsigned j = 0; j < local.size(); j++) {
				lItem = local.at(j);
				tempItem = lItem;
				if(lItem.find("[") != string::npos)
					tempItem = lItem.substr(0, lItem.find("["));
				if(item.compare(tempItem) == 0)
					local_definitions += type + " " + lItem + " = " + initialValue + ";\n";
			}
		}
	}
}

/**
 * This method finds and assigns shared variables to be
 * made global variables in the output file
 */
bool assignGlobalDefs(string fileName) {
	ifstream iFile(fileName);
	string line;

	// General global definitions
	global_definitions 	+= "const int MAX_THREADS = " + total_threads + ";\n\n"
						+ "// Adding Thread Data Structure\n"
						+ string("struct tdata {\n")
						+ string("\tint id, start, end;\n")
						+ string("};\n\n")
						+ string("// Thread data array to track threads\n")
						+ string("struct tdata tdata_arr[MAX_THREADS];\n\n")
						+ string("// Parallel function declaration\n")
						+ string("\nvoid *parFunc(void *params);\n");

	if(iFile.is_open()) {
		while(getline(iFile, line)) {

			// Looking for shared() directive
			if(line.find("shared") != string::npos) {
				line.erase(0, line.find("shared(") + 7);
				string sharedVariables = line.substr(0, line.find(")"));
				while ((sharedVariables.find(",")) != std::string::npos) {
					shared_vars.push_back(sharedVariables.substr(0, sharedVariables.find(",")));
					sharedVariables.erase(0, sharedVariables.find(",") + 1);
				}
				shared_vars.push_back(sharedVariables);
			}
		}
		iFile.close();
	}

	// Found some shared variables, sending them to global definitions
	if(shared_vars.size() > 0) {
		iFile.open(fileName.c_str());
		if(iFile.is_open()) {
			while(getline(iFile, line)) {
				if(line.find("int main") != string::npos || line.find("void main") != string::npos) {
					global_definitions += "\n//Adding shared variables\n";
					while(getline(iFile, line)) {
						if(line.find("int ") != string::npos)
							addToDefs("global", line, "int", "0");
						else if(line.find("char ") != string::npos)
							addToDefs("global", line, "char", "''");
						else if(line.find("bool ") != string::npos)
							addToDefs("global", line, "bool", "true");
						else if(line.find("float ") != string::npos)
							addToDefs("global", line, "float", "0");
						else if(line.find("string ") != string::npos)
							addToDefs("global", line, "string", "");
					}
				}

			}
		}
	}

	iFile.close();
	return true;
}

/**
 * This method extracts and assigns local variables
 * to be used in the output program
 */
bool assignLocalDefs(string fileName) {

	ifstream iFile(fileName);
	string line;

	if(iFile.is_open()) {
		while(getline(iFile, line)) {
			// Looking for private() variables
			if(line.find("private") != string::npos) {
				line.erase(0, line.find("private(") + 8);
				string local_variables = line.substr(0, line.find(")"));
				while ((local_variables.find(",")) != std::string::npos) {
					local_vars.push_back(local_variables.substr(0, local_variables.find(",")));
					local_variables.erase(0, local_variables.find(",") + 1);
				}
				local_vars.push_back(local_variables);
			}
		}
		iFile.close();
	}

	iFile.open(fileName.c_str());

	if(iFile.is_open()) {
		while(getline(iFile, line)) {
			if(line.find("int main") != string::npos || line.find("void main") != string::npos) {
				local_definitions += "\n//Adding local variables\n";
				while(getline(iFile, line)) {
					if(line.find("int ") != string::npos)
						addToDefs("local", line, "int", "0");
					else if(line.find("char ") != string::npos)
						addToDefs("local", line, "char", "''");
					else if(line.find("bool ") != string::npos)
						addToDefs("local", line, "bool", "true");
					else if(line.find("float ") != string::npos)
						addToDefs("local", line, "float", "0");
					else if(line.find("string ") != string::npos)
						addToDefs("local", line, "string", "");
				}
			}

		}
	}

	iFile.close();
	return true;
}


/**
 * This method replaces the omp_get_thread_num
 * directive in the program.
 */
string replaceGetThreadNum(string fileName) {
	ifstream iFile(fileName);
	string line, tmpLine;
	bool shouldEdit = false;
	while (getline(iFile, line)) {
		// Looking for omp_get_thread_num() directive
		if (line.find(std::string("omp_get_thread_num()")) != std::string::npos) {
			shouldEdit = true;
			int pos = line.find(std::string("omp_get_thread_num()"));

			// Replacing the directive with custom thread_id variable
			string temp = line.substr(0, pos) + " thread_id " + line.substr(pos + 20);

			tmpLine += temp + "\n";
		}
		else
			tmpLine += line + "\n";
	}

	iFile.close();

	if(shouldEdit) {
		ofstream tmpFile(outputFileName);
		tmpFile << tmpLine;
		tmpFile.close();
		return outputFileName;
	}
	else
		return fileName;
}


/**
 * This method creates the thread creation block
 */
bool assignThreadCreation(string fileName) {
	ifstream iFile(fileName);
	string line, threadNum, maxIterations;
	bool status = false;

	if(iFile.is_open()) {
		while(getline(iFile, line)) {
			// Looking for any parallel for loop
			if(line.find("omp for") != string::npos || line.find("omp parallel for") != string::npos) {
				getline(iFile, line);
				if(line.find("{") != string::npos)
					getline(iFile, line);

				// Extracting starting and ending indexes
				string iter_start = line.substr(0, line.find(";"));
				iter_start = iter_start.substr(iter_start.find_first_of("0123456789"));

				string line_part = line.substr(line.find(";") + 1);

				string iter_end = line_part.substr(0, line_part.find(";"));
				iter_end = iter_end.substr(iter_end.find_first_of("0123456789"));

				if(line.find("<=") != string::npos)
					iter_end += " + 1";

				string iter_per_thread = "( " + iter_end + " - " + iter_start + " ) / " + total_threads;
				string extra_iters = "( " + iter_end + " - " + iter_start + " ) % " + total_threads;

				// Writing the thread creation body keeping
				// in mind the for loop
				thread_creation = string("\tpthread_t threads[MAX_THREADS];\n")
								+ string("\tint iter_per_thread, iter_incr, extra_iters;\n\n")
								+ "\titer_per_thread = " + iter_per_thread + ";\n"
								+ string("\titer_incr = 0;\n")
								+ "\textra_iters = " + extra_iters + ";\n\n"
								+ string("\tfor(int i = 0; i < MAX_THREADS; i++)\n\t{\n")
								+ string("\t\ttdata_arr[i].id = i;\n")
								+ "\t\ttdata_arr[i].start = " + iter_start + " + iter_incr;\n"
								+ string("\t\tif( extra_iters > 0) {\n")
								+ string("\t\t\ttdata_arr[i].end = tdata_arr[i].start + iter_per_thread + 1;\n")
								+ string("\t\t\titer_incr += iter_per_thread + 1;\n")
								+ string("\t\t\textra_iters--;\n")
								+ string("\t\t}\n")
								+ string("\t\telse {\n")
								+ string("\t\t\ttdata_arr[i].end = tdata_arr[i].start + iter_per_thread;\n")
								+ string("\t\t\titer_incr += iter_per_thread;\n")
								+ string("\t\t}\n")
								+ string("\t\tint rc = pthread_create(&threads[i], NULL, parFunc, (void *)&tdata_arr[i]);\n")
								+ string("if(rc != 0)\n")
								+ string("printf(\"Failed to created thread!\");\n")
								+ string("\t}\n\n")
								+ string("\tfor(int i = 0; i < MAX_THREADS; i++) {\n")
								+ string("\t\tpthread_join(threads[i], NULL);\n")
								+ string("\t}\n");
				status = true;
			}
			// Looking for just the parrallel construct
			else if(	line.find("omp parallel s") != string::npos ||
						line.find("omp parallel p") != string::npos ||
						line.find("omp parallel n") != string::npos ) {

				// Writing the thread creation body and discarding any
				// loop definition logic
				thread_creation = string("\tpthread_t threads[MAX_THREADS];\n\n")
								+ string("\tfor(int i = 0; i < MAX_THREADS; i++)\n\t{\n")
								+ string("\t\ttdata_arr[i].id = i;\n\n")
								+ string("\t\tint rc = pthread_create(&threads[i], NULL, parFunc, (void *)&tdata_arr[i]);\n")
								+ string("if(rc != 0)\n")
								+ string("\tprintf(\"Failed to created thread!\");\n")
								+ string("\t}\n")
								+ string("\tfor(int i = 0; i < MAX_THREADS; i++) {\n")
								+ string("\t\tpthread_join(threads[i], NULL);\n")
								+ string("\t}\n");
				status = true;
			}
		}
		iFile.close();
	}

	return status;
}

/**
 * Utility function to compare variables in shared variable vector
 */
bool compareVars(string var) {
	bool isPresent = false;
	var = removeSpaces(var);
	for(unsigned i = 0; i < shared_vars.size(); i++) {
		string sVar = shared_vars.at(i);
		if(var.find("[") != string::npos)
			var = var.substr(0, var.find("["));
		if(sVar == var)
			return true;
	}
	return isPresent;
}

/**
 * This method extracts variables defined within main body
 */
string extractMainVars(string line) {
	string temp;
	if(line.find("int ") != string::npos) {
		temp += line.substr(0, line.find("int ") + 4);
		line.erase(0, line.find("int ") + 4);
	}
	else if(line.find("float ") != string::npos) {
		temp += line.substr(0, line.find("float ") + 5);
		line.erase(0, line.find("float ") + 5);
	}
	else if(line.find("bool ") != string::npos) {
		temp += line.substr(0, line.find("bool ") + 5);
		line.erase(0, line.find("bool ") + 5);
	}
	else if(line.find("char ") != string::npos) {
		temp += line.substr(0, line.find("char ") + 5);
		line.erase(0, line.find("char ") + 5);
	}
	else if(line.find("string ") != string::npos) {
		temp += line.substr(0, line.find("string ") + 7);
		line.erase(0, line.find("string ") + 7);
	}
	else {
		temp += "//Unknown data type [Supported types are int, float, bool, char, string]";
		return temp;
	}

	line.erase(line.find(";"), 1);

	while(line.find(",") != string::npos) {
		string var = line.substr(0, line.find(","));
		line.erase(0, line.find(",") + 1);
		if(!compareVars(var))
			temp += var + ",";
	}
	if(!compareVars(line))
		temp += line;
	else
		temp.erase(temp.find_last_of(","), 1);
	temp += ";";
	return temp;
}

/**
 * This function extracts and assigns the extra functions
 * other than the main function in the input
 */
bool assignExtraFuncs(string fileName) {
	ifstream iFile(fileName);
	string line, funcDef;
	bool status = false;
	if(iFile.is_open()) {
		while(getline(iFile, line)) {
			if(	(	line.find("void ") != string::npos ||
					line.find("int ") != string::npos ||
					line.find("float ") != string::npos ||
					line.find("string ") != string::npos ||
					line.find("bool ") != string::npos
				)
				&& line.find("main") == string::npos
				&& line.find("(") != string::npos)
			{
				int bracesCounter = 0;

				// Adding extra param to a function just in case
				// it was called by a section
				if(hasSections) {
					string temp;
					if(line.find("()") != string::npos)
						temp = line.substr(0, line.find_last_of(")")) + "int thread_id)\n";
					else
						temp = line.substr(0, line.find_last_of(")")) + ", int thread_id)\n";

					extra_functions += temp;
				}
				else
					extra_functions += line + "\n";

				while(true) {
					getline(iFile, line);
					if(line.find("{") != string::npos)
						bracesCounter++;
					else if(line.find("}") != string::npos)
						bracesCounter--;

					extra_functions += line + "\n";

					if(bracesCounter == 0)
						break;
				}
			}
		}
	}
	iFile.close();
	return status;
}

/**
 * This method extracts and assigns the main body
 */
bool assignMainBody(string fileName) {
	ifstream iFile(fileName);
	string line;
	int bracesCounter = 0;
	bool status = false;

	if(iFile.is_open()) {
		while(getline(iFile, line)) {
			if(line.find("main(") != string::npos) {
				main_body += extra_functions + "\n";

				main_body += line + "\n { \n";

				if(!thread_vars.empty()) {
					main_body += "//Initializing thread variables(mutexes and barriers)\n"
								+ thread_vars + "\n";
				}

				while(true) {
					getline(iFile, line);
					if(line.find("{") != string::npos)
						bracesCounter++;
					else if(line.find("}") != string::npos)
						bracesCounter--;
					else if(line.find("#pragma omp parallel") != string::npos && line.find("parallel for") == string::npos) {
						int innerBraces = 0;
						main_body += thread_creation + "\n";
						while(true) {
							getline(iFile, line);
							if(line.find("{") != string::npos)
								innerBraces++;
							else if(line.find("}") != string::npos)
								innerBraces--;
							if(innerBraces == 0)
								break;
						}
					}
					// Looking for for loop construct
					else if(line.find("#pragma omp parallel for") != string::npos) {
						int innerBraces = 0;
						main_body += thread_creation + "\n";
						getline(iFile, line);
						while(true) {
							getline(iFile, line);
							if(line.find("{") != string::npos)
								innerBraces++;
							else if(line.find("}") != string::npos)
								innerBraces--;
							if(innerBraces == 0)
								break;
						}
					}
					else if(line.find("int ") != string::npos || line.find("float ") != string::npos
								|| line.find("char ") != string::npos || line.find("bool ") != string::npos
								|| line.find("string ") != string::npos) {
						main_body += extractMainVars(line) + "\n";
					}
					else
						main_body += line + "\n";

					if(bracesCounter == 0) {
						status = true;
						main_body += "}\n";
						break;
					}
				}
			}
		}
	}
	return status;
}

/**
 * This method assigns the whole body of the parallel
 * function that is to be executed by the threads.
 */
bool assignParFuncBody(string fileName) {
	ifstream iFile(fileName);
	string line;
	bool status = false;
	par_func_body	+= string("\tstruct tdata *this_thread_data;\n")
					+ string("\tthis_thread_data = (struct tdata *)params;\n")
					+ string("\tint thread_id = this_thread_data->id;\n");
	if(hasForLoop)
		par_func_body 	+= string("\tint start = this_thread_data->start;\n")
						+ string("\tint end = this_thread_data->end;\n");

	if(iFile.is_open()) {
		while (getline(iFile, line)) {
			if (line.find("#pragma omp parallel") != string::npos && line.find("for") == string::npos) {
				int bracesCounter = 0;
				while (true) {
					getline(iFile, line);
					if(line.find("{") != string::npos)
						bracesCounter++;
					else if(line.find("}") != string::npos)
						bracesCounter--;
					else if(line.find("#pragma") != string::npos &&
							line.find("barrier") == string::npos) {
						int innerBraces = 0;

						// Check for for
						if(line.find("for") != string::npos)
							par_func_body += for_loop_body + "\n";
						// Check for critical
						if(line.find("critical") != string::npos)
							par_func_body += critical_block + "\n";
						// Check for single
						if(line.find("single") != string::npos)
							par_func_body += single_block + "\n";
						// Check for sections
						if(line.find("sections") != string::npos)
							par_func_body += section_block + "\n";

						while(true) {
							getline(iFile, line);
							if(line.find("{") != string::npos)
								innerBraces++;
							else if(line.find("}") != string::npos)
								innerBraces--;
							else if(line.find("for") != string::npos) {
								int innerForBraces = 0;
								while(true) {
									getline(iFile, line);
									if(line.find("{") != string::npos)
										innerForBraces++;
									else if(line.find("}") != string::npos)
										innerForBraces--;
									if(innerForBraces == 0)
										break;
								}
							}
							if(innerBraces == 0)
								break;
						}
					}
					else if(line.find("#pragma omp barrier") != string::npos)
						par_func_body += barrier_body + "\n";
					else
						par_func_body += line + "\n";
					if(bracesCounter == 0)
						break;
				}
			}
			else if (line.find("#pragma omp parallel") != string::npos && line.find("for") != string::npos)
				par_func_body += for_loop_body + "\n";

		}
	}

	iFile.close();
	return status;
}

/**
 * This method assigns the for loop body to be substituted
 * in the output file.
 */
bool assignForBody(string fileName) {
	ifstream iFile(fileName);
	bool status = false;
	string line;
	int bracesCounter = 0;
	if(iFile.is_open()) {
		while(getline(iFile, line)) {
			if(line.find("#pragma") != string::npos && line.find("for") != string::npos) {
				hasForLoop = true;
				for_loop_body += string("\tfor( i = start; i < end; i++)\n");
				getline(iFile, line);
				while(true) {
					getline(iFile, line);
					if(line.find("{") != string::npos) {
						bracesCounter++;
						for_loop_body += line + "\n";
					}
					else if(line.find("}") != string::npos) {
						bracesCounter--;
						for_loop_body += line + "\n";
					}
					else
						for_loop_body += line + "\n";
					if(bracesCounter == 0) {
						status = true;
						break;
					}
				}
			}
		}
	}

	iFile.close();
	return status;
}

/**
 * This method extracts and assigns the critical section block
 */
bool assignCriticalBlock(string fileName) {
	ifstream iFile(fileName);
	string line;
	bool status = false;

	while (getline(iFile, line)) {
		if (line.find(string("omp critical")) != std::string::npos) {
			int innerBracesCounter = 0;
			global_definitions += "\n//Critical Construct found\n"
								+ string("pthread_mutex_t crit_mux;\n");

			critical_block += string("pthread_mutex_lock(&crit_mux);\n");

			while(true) {
				getline(iFile, line);
				if(line.find(string("{")) != string::npos)
					innerBracesCounter++;
				else if(line.find(string("}")) != string::npos)
					innerBracesCounter--;

				critical_block += line + "\n";

				if(innerBracesCounter == 0) {
					critical_block += string("pthread_mutex_unlock(&crit_mux);\n");
					status = true;
					break;
				}

			}
		}
	}
	iFile.close();

	return status;
}

/**
 * This method extracts and assigns the single directive block
 */
bool assignSingleBlock(string fileName) {
	ifstream iFile(fileName);
	string line;
	bool status = false;
	if(iFile.is_open()) {
		while(getline(iFile, line)) {
			if(line.find("#pragma") != string::npos && line.find("single") != string::npos) {
				int bracesCounter = 0;

				// Add single construct definitions to global definitions
				global_definitions += "\n//Single Construct found\n"
									+ string("pthread_mutex_t single_mux;\n")
									+ string("pthread_barrier_t single_c_barr;\n")
									+ string("bool single_thread_flag = true;\n");

				thread_vars += "pthread_barrier_init(&single_c_barr, NULL, MAX_THREADS);\n";

				single_block 	+= "pthread_mutex_lock(&single_mux);\n"
								+ string("if(single_thread_flag) {\n");
				while(true) {
					getline(iFile, line);
					if(line.find("{") != string::npos)
						bracesCounter++;
					else if(line.find("}") != string::npos)
						bracesCounter--;
					else
						single_block += line + "\n";
					if(bracesCounter == 0) {
						single_block 	+= "single_thread_flag = false;\n}\n"
										+ string("pthread_mutex_unlock(&single_mux);\n")
										+ string("pthread_barrier_wait(&single_c_barr);\n");
						status = true;
						break;
					}
				}
			}
		}
	}
	return status;
}

/**
 * This method extracts and assigns the barrier directive block
 */
bool assignBarrierBlock(string fileName) {
	ifstream iFile(fileName);
	string line;
	bool status = false;
	if(iFile.is_open()) {
		while(getline(iFile, line)) {
			if(line.find("omp barrier") != string::npos) {
				global_definitions += string("// Barrier construct found\n")
									+ string("pthread_barrier_t barrier;\n\n");
				thread_vars += "pthread_barrier_init(&barrier, NULL, MAX_THREADS);\n";
				barrier_body += "pthread_barrier_wait(&barrier);\n";
				status = true;
				break;
			}
		}
	}
	return status;
}

/**
 * This method extracts and assigns the sections directive block
 */
bool assignSectionsBlock(string fileName) {
	ifstream iFile(fileName);
	string line;
	bool status = false;
	vector<string> inSectionFucs;
	if(iFile.is_open()) {
		while(getline(iFile, line)) {
			if(line.find("#pragma") != string::npos && line.find("sections") != string::npos) {
				int bracesCounter = 0;
				int sectionCounter = 0;

				section_block = "pthread_mutex_lock(&sec_mux);\n{\n";

				while(true) {
					getline(iFile, line);
					if(line.find("{") != string::npos)
						bracesCounter++;
					else if(line.find("}") != string::npos)
						bracesCounter--;

					if(line.find("#pragma") != string::npos && line.find("section") != string::npos) {
						int innerBracesCounter = 0;
						string section = "";
						hasSections = true;
						while(true) {
							getline(iFile, line);
							if(line.find("{") != string::npos)
								innerBracesCounter++;
							else if(line.find("}") != string::npos)
								innerBracesCounter--;
							else {
								string temp;
								if(line.find("()") != string::npos)
									temp = line.substr(0, line.find_last_of(")")) + "thread_id);";
								else
									temp = line.substr(0, line.find_last_of(")")) + ", thread_id);";

								section += temp + "\n";
							}

							if(innerBracesCounter == 0) {
								if(sectionCounter == 0)
									section_block += "if";
								else
									section_block += "else if";
								section_block += "(sec_exec[" + std::to_string(sectionCounter) + "] == true)\n"
												+ "{\n"
												+ section
												+ "sec_exec[" + std::to_string(sectionCounter) + "] = false;\n"
												+ "}\n";
								break;
							}
						}
						sectionCounter++;
					}

					if(bracesCounter == 0) {
						global_definitions += string("//Sections construct found\n")
											+ "pthread_mutex_t sec_mux;\n"
											+ "const int SEC_COUNT = " + std::to_string(sectionCounter) + ";\n"
											+ "bool sec_exec[SEC_COUNT];\n";
						section_block += "}\npthread_mutex_unlock(&sec_mux);\n";
						thread_vars += string("for(int i=0; i < SEC_COUNT; i++)\n")
									+	string("sec_exec[i] = true;\n");
						status = true;
						break;
					}
				}
			}
		}
	}
	iFile.close();
	return status;
}

/**
 * Utility method to remove tab spaces in a string
 */
string stripTabs(string line) {
	while(line.find("\t") != string::npos)
		line.erase(line.find("\t"), 1);
	return line;
}

/**
 * Utility method to organize the output of the file
 */
void organize(string fileName) {
	ifstream file(fileName);
	string line, temp, newFile;
	int bracesCounter = 0, tempCounter = 0;
	if(file.is_open()) {
		while(getline(file, line)) {
			if(!line.empty()) {
				if(line.find("#include") == string::npos) {
					if(line.find("}") != string::npos)
						bracesCounter--;
					line = stripTabs(line);
					temp = "";
					for(tempCounter = 0; tempCounter < bracesCounter; tempCounter++)
						temp += "\t";
					newFile += temp + line + "\n";
					if(line.find("{") != string::npos)
						bracesCounter++;

				}
				else {
					newFile += line + "\n";
				}
			}
		}
	}
	file.close();
	ofstream oFile(fileName);
	oFile << newFile << endl;
	oFile.close();
}

// Main function
int main(int argc, char* argv[])
{
	if(string(argv[1]) == "") {
		cout << "Please provide an input file!" << endl;
	}
	else {
		string fileName = string(argv[1]);
		string name;
		if(fileName.find("/") != string::npos)
			name = fileName.substr(fileName.find_last_of("/") + 1);
		else
			name = fileName;

		if(name.find(".") != string::npos)
			name.erase(name.find("."));

		outputFileName = name + "_construct.cc";

		time_t timer = time(NULL);
		printf("Process started at: %s", ctime(&timer));
		cout << "Input File Name: " << fileName << endl << endl;

		cout << "Replacing 'omp_get_num_thread' directive." << endl;
		fileName = replaceGetThreadNum(fileName);

		cout << "Finding total threads to create." << endl;
		assignTotalThreads(fileName);

		cout << "Replacing 'omp.h' with 'pthread.h'." << endl;
		assignHeader(fileName);

		cout << "Calculating global variables." << endl;
		assignGlobalDefs(fileName);

		cout << "Calculating local variables." << endl;
		assignLocalDefs(fileName);

		cout << "Writing thread creation code." << endl;
		assignThreadCreation(fileName);

		cout << "Checking for special constructs declarations:" << endl;
		if(assignForBody(fileName))
			cout << "\tFound 'for' construct" << endl;
		if(assignCriticalBlock(fileName))
			cout << "\tFound 'critical' construct" << endl;
		if(assignSingleBlock(fileName))
			cout << "\tFound 'single' construct" << endl;
		if(assignBarrierBlock(fileName))
			cout << "\tFound 'barrier construct" << endl;
		if(assignSectionsBlock(fileName))
			cout << "\tFound 'sections' construct" << endl;

		cout << "Writing parallel function definition." << endl;
		assignParFuncBody(fileName);

		cout << "writing additional functions." << endl;
		assignExtraFuncs(fileName);

		cout << "Writing main body function." << endl;
		assignMainBody(fileName);

		cout << "Printing the output to file." << endl;
		ofstream oFile(outputFileName);

		oFile << headers << endl << global_definitions << endl << main_body << endl;
		oFile << "void *parFunc(void *params)\n{" << local_definitions << endl << par_func_body << endl;
		oFile << "\tpthread_exit(NULL);\n}";
		oFile.close();

		cout << "Re-organizing the file." << endl;
		organize(outputFileName);

		cout << endl << "Output file name: " << outputFileName << endl;

		timer = time(NULL);
		printf("Process completed at: %s\n", ctime(&timer));
	}
	return 0;
}
