/*
	guava
	-----
	a simple unix shell
	all basic commands runnable
	
	Aksel Taylan, Fall 2018

*/


#include <iostream>
#include <string.h>
#include <stdio.h>
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <boost/tokenizer.hpp>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
using namespace std;
using namespace boost;

// GLOBAL VARIABLES
vector<pid_t> pids = {};

void execute(char* const* args) {
	execvp(args[0], args);
}

int process(vector<string> tokens) {

	// get command (always the first token)
	string start = tokens[0];

	if (start == "pwd") {
		char buffer[1000];
		if (getcwd(buffer, sizeof(buffer)) != NULL) {
			cout << buffer << endl;
		}
		else {
			perror("Error");
		}
		return 0;
	}

	if (start == "cd") {
		const char *path = tokens[1].c_str();
		string backslash = "..";
		const char *minus = backslash.c_str();
		if (*path == '-') {

			path = minus;
		}
		int ret = chdir(path);
		if (ret == -1) {
			perror("Error");
		}
		return 0;
	}

	if (start == "jobs") {
		pid_t pid = fork();

		if (pid == -1) {
			perror("Error");
		}
		else if (pid == 0) {
			cout << endl;
			cout << pids.size() << endl;
			for (int i = 0; i < pids.size(); ++i) {
				if (kill(pids[i],0) == 0) {
					cout << pids[i] << endl;
				}
			}
		}
		return 0;
	}

	vector<string> args;
	args.push_back(start);
	int i = 1;
	while (i < tokens.size()) {
		if (tokens[i] == "|") break;
		args.push_back(tokens[i]);
		++i;
	}
	const int argsize = args.size();
	const char* argvv[argsize+1];
	for (int j = 0; j < argsize; ++j) {
		argvv[j] = args[j].c_str();
	}
	argvv[argsize] = nullptr;
	char* const* argv = const_cast<char* const*>(argvv);
	execute(argv);
	return 0;
}

int processCmd(vector<string> command, bool& bg) {
	int finput = 0;
	int fout = 0;
	int s_in = 0;
	int s_out = 0;
	int s_err = 0;
	string ifile; string ofile;
	string cmd = command[0];
	bool redirect = false;

	for (int i = 1; i < command.size(); ++i) {
		if (command[i] == "<") {
			finput = 1;
			ifile = command[i+1];
			command.erase(command.begin()+i);
			command.erase(command.begin()+i+1);
			redirect = true;
			break;
		}
		if (command[i] == ">") {
			fout = 1;
			ofile = command[i+1];
			command.erase(command.begin()+i);
			command.erase(command.begin()+i+1);
			redirect = true;
			break;
		}
		if (command[i] == "&") {
			bg = true;
			command.erase(command.begin()+i);
		}
	}

	int in; int out; int err;
	if (redirect) {
		if (finput == 1) {
			in = open(ifile.c_str(), O_RDONLY);
			if (in < 0) {
				perror("Error");
			}
			else {
				s_in = dup(0);
				close(0);
				dup2(in, 0);
			}
		}
		if (fout == 1) {
			out = open(ofile.c_str(), O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWGRP | S_IWUSR);
			s_out = dup(1);
			close(1);
			dup2(out, 1);
		}
	}

	int ret = process(command);
	if (ret == -1) {
		return -1;
	}

	if (finput == 1) {
		close(in);
		dup2(s_in, 0);
	}
	if (fout == 1) {
		close(out);
		dup2(s_out, 1);
	}

	return 0;

}

vector<vector<string>> generateCommands(vector<string> tokens) {
	vector<vector<string>> commands;
	vector<string> newCommands;
	for (int i = 0; i < tokens.size(); ++i) {
		if (tokens[i] != "|") {
			newCommands.push_back(tokens[i]);
		}
		else {
			commands.push_back(newCommands);
			newCommands.clear();
		}
	}
	commands.push_back(newCommands);
	return commands;
}

int main(int argc, char ** argv)  {

	bool prompt = true;
	int c;
	int tot = 0;

	while ((c = getopt(argc, argv, "t")) != -1) {
	    switch(c) {
	      case 't':
	        prompt = false;
	        break;
	      case '?':
	        cout << "unknown input" << endl;
	        return 1;
	      default:
	        abort();
	    }
  	}	

	int cur_in = dup(0);
	int cur_out = dup(1);

	while (true) {

		dup2(cur_in, 0);
		dup2(cur_out, 1);

		string input;
		vector< string > tokens;
		vector< string > final_tokens;
		if (prompt) cout << "guava$ ";

		// initial tokenize pass
		getline(cin, input);
		typedef tokenizer<char_separator<char> > tokenizer;
		char_separator<char> sep(" ", "|<>&", drop_empty_tokens);
		tokenizer tok(input, sep);

		for (const auto &t : tok) {
			tokens.push_back(t);
		}

		// next tokenize pass
		for (int i = 0; i < tokens.size(); ++i) {

			// if the token starts with a single quote
			if (tokens[i][0] == '\'') {
				string argtoken;
				int j = i + 1;

				// if the end of this quote isn't in the current token,
				// combine all tokens after this one until the end quote is found
				if (tokens[i][tokens[i].length()-1] != '\'') {
					tokens[i].erase(remove(tokens[i].begin(), tokens[i].end(), '\''), tokens[i].end());
					argtoken = tokens[i];
					while (tokens[j][tokens[j].length()-1] != '\'') {
						argtoken += " ";
						argtoken += tokens[j];
						++j;
					}
					argtoken += " ";
					tokens[j].erase(remove(tokens[j].begin(), tokens[j].end(), '\''), tokens[j].end());
					argtoken += tokens[j];
					i = j;
				}
				// if both the first quote and final quote are in this token, remove them
				else {
					tokens[i].erase(remove(tokens[i].begin(), tokens[i].end(), '\''), tokens[i].end());
					argtoken = tokens[i];
				}
				final_tokens.push_back(argtoken);
			}
			// if the token starts with a double quote
			else if (tokens[i][0] == '\"') {
				string argtoken;
				int j = i + 1;

				// if the end of this quote isn't in the current token,
				// combine all tokens after this one until the end quote is found
				if (tokens[i][tokens[i].length()-1] != '\"') {
					tokens[i].erase(remove(tokens[i].begin(), tokens[i].end(), '\"'), tokens[i].end());
					argtoken = tokens[i];
					while (tokens[j][tokens[j].length()-1] != '\"') {
						argtoken += " ";
						argtoken += tokens[j];
						++j;
					}
					argtoken += " ";
					tokens[j].erase(remove(tokens[j].begin(), tokens[j].end(), '\"'), tokens[j].end());
					argtoken += tokens[j];
					i = j;
				}
				// if both the first quote and final quote are in this token, remove them
				else {
					tokens[i].erase(remove(tokens[i].begin(), tokens[i].end(), '\"'), tokens[i].end());
					argtoken = tokens[i];
				}
				final_tokens.push_back(argtoken);
			}
			else {
				string argtoken;
				argtoken = tokens[i];
				final_tokens.push_back(argtoken);
			}
		}

		vector<vector<string>> commands = generateCommands(final_tokens);

		// if there are pipes
		pid_t pid;
		if (commands.size() >= 1) {
			int num_pipes = commands.size() - 1; 
			int s;
			int d = dup(0);

			for (int i = 0; i < num_pipes; ++i) {
				bool bg = false;
				int fd[2];
				pipe(fd);
				pid = fork();

				if (pid == 0) {
					// every other command, reap zombie processes
					if (tot % 2 == 0) {
						for (int i = 0; i < pids.size(); ++i) {
							int* ret;
							int res = waitpid(pids[i], ret, WNOHANG);
							if (res != -1) {
								//wait(0);
								int res = waitpid(pids[i], ret, 0);
								pids.erase(pids.begin()+i);
							}
						}
					}
					dup2(fd[1],1);
					close(fd[0]);
					processCmd(commands[i], bg);
				}
				else {
					if (!bg) {
						wait(0);
					}
					else {
						pids.push_back(pid);
					}
					close(fd[1]);
					dup2(fd[0], 0);
				}
			}

			bool bg = false;
			if ((pid = fork()) == 0) {
				processCmd(commands[commands.size()-1], bg);
			}
			else {
				if (!bg) {
					while (wait(&s) != pid);
				}
				else {
					pids.push_back(pid);
				}
			}
			dup2(d, 0);
		}

		++tot;
	}

	return 0;
}