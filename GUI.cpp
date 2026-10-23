//© 潘家睿 20261019
#include "raylib\\include\\raylib.h"
#include "favicon.h"
#include <stack>
#include <string>
#include <vector>
#include <cstring>
#include <fstream>
#include <cctype>
#include <cmath>
#include <stdexcept>
#include <unordered_map>
#include <iostream>
#include <cmath>
#include <processthreadsapi.h>
#include <windef.h>
#include <minwinbase.h>
#include <synchapi.h>
#include <winbase.h>
#include <quadmath.h>
#include <filesystem>
#include <complex>
#include <regex>
const int N = 1e6 + 10;
namespace fs = std::filesystem;
bool make_file(std::string_view name) {
	try {
		fs::path file_path(name);

		// 如果包含路径，先提取并创建父级目录（如 path/to/）
		if (file_path.has_parent_path()) {
			fs::create_directories(file_path.parent_path());
		}

		// 创建并打开文件
		std::ofstream file(file_path);

		// 检查文件是否成功打开
		if (!file.is_open()) {
			std::cerr << "错误：无法创建文件 -> " << name << std::endl;
			return false;
		}

		file.close();
		return true;

	} catch (const fs::filesystem_error& e) {
		std::cerr << "文件系统错误: " << e.what() << std::endl;
		return false;
	}
}
bool is_exist(const std::string& filename) {
	return std::filesystem::exists(filename);
}

using f128 = __float128;
using Complex = std::complex<f128>;

class calculator {
public:
	enum class AngleMode { DEGREES, RADIANS, GRADIANS };

	static std::string calculate(std::string_view expr) {
		auto tokens = tokenize(expr);
		auto postfix = infixToPostfix(tokens);
		Complex result = evaluatePostfix(postfix);
		return formatComplex(result);
	}

	static void setAngleMode(AngleMode mode) {
		currentAngleMode = mode;
	}

	static bool isNumber(const std::string &s) {
		if (s.empty()) return false;
		std::regex complexRegex(R"(^[+-]?(?:\d+(?:\.\d*)?|\.\d+)?(?:[+-](?:\d+(?:\.\d*)?|\.\d+)?i)?$)");
		std::regex pureImaginaryRegex(R"(^[+-]?(?:\d+(?:\.\d*)?|\.\d+)?i$)");
		if (std::regex_match(s, complexRegex) || std::regex_match(s, pureImaginaryRegex)) {
			return true;
		}
		if (s == "_") return false;
		return false;
	}

	static Complex parseComplex(const std::string& s) {
		if (s == "i" || s == "+i") return Complex(0.0Q, 1.0Q);
		if (s == "-i") return Complex(0.0Q, -1.0Q);
		if (s.find('i') == std::string::npos) {
			return Complex(strtoflt128(s.c_str(), nullptr), 0.0Q);
		}
		if (s.back() == 'i' && s.find_first_of("+-", 1) == std::string::npos && s != "+" && s != "-") {
			std::string imStr = s.substr(0, s.size() - 1);
			if (imStr.empty() || imStr == "+") return Complex(0.0Q, 1.0Q);
			if (imStr == "-") return Complex(0.0Q, -1.0Q);
			return Complex(0.0Q, strtoflt128(imStr.c_str(), nullptr));
		}
		f128 realPart = 0.0Q;
		f128 imagPart = 0.0Q;
		size_t signPos = s.find_last_of("+-");
		if (signPos != std::string::npos && signPos > 0) {
			std::string realStr = s.substr(0, signPos);
			std::string imagStr = s.substr(signPos, s.size() - signPos - 1);
			realPart = strtoflt128(realStr.c_str(), nullptr);
			if (imagStr == "+") imagPart = 1.0Q;
			else if (imagStr == "-") imagPart = -1.0Q;
			else imagPart = strtoflt128(imagStr.c_str(), nullptr);
		} else {
			std::string imStr = s.substr(0, s.size() - 1);
			imagPart = strtoflt128(imStr.c_str(), nullptr);
		}
		return Complex(realPart, imagPart);
	}

	static bool isValidExpression(std::string_view expr, std::string& errorMsg) {
		if (expr.empty()) {
			errorMsg = "算式不能为空";
			return false;
		}
		int bracketCount = 0;
		char lastChar = '\0';
		for (size_t i = 0; i < expr.size(); ++i) {
			char c = expr[i];
			if (isspace(c)) continue;
			if (c == '(') {
				bracketCount++;
			} else if (c == ')') {
				bracketCount--;
				if (bracketCount < 0) {
					errorMsg = "错误: 发现多余的右括号 \')\'";
					return false;
				}
			}
			if (c == '+' || c == '*' || c == '/' || c == '^') {
				if (lastChar == '+' || lastChar == '-' || lastChar == '*' || lastChar == '/' || lastChar == '^') {
					errorMsg = "错误: 连续的运算符 \'" + std::string(1, lastChar) + c + "\'";
					return false;
				}
				if (lastChar == '\0' || lastChar == '(') {
					errorMsg = "错误: 运算符 \'" + std::string(1, c) + "\' 缺少左操作数";
					return false;
				}
			}
			lastChar = c;
		}
		if (lastChar == '+' || lastChar == '-' || lastChar == '*' || lastChar == '/' || lastChar == '^') {
			errorMsg = "错误: 算式不能以运算符 \'" + std::string(1, lastChar) + "\' 结尾";
			return false;
		}
		if (bracketCount != 0) {
			errorMsg = "错误: 括号未闭合";
			return false;
		}
		return true;
	}

	static void storeMemory(Complex value) {
		memoryValue = value;
	}
	static Complex recallMemory() {
		return memoryValue;
	}

private:
	static bool signbitf(f128 __x) {
		return __x < 0.0Q ? true : false;
	}
	struct Token {
		std::string value;
		bool isOperator;
	};

	static std::vector<Token> tokenize(std::string_view expr) {
		std::vector<Token> tokens;
		bool canBeUnary = true;
		for (size_t i = 0; i < expr.size();) {
			if (isspace(expr[i])) {
				++i;
				continue;
			}
			std::string sub = std::string(expr.substr(i));
			std::smatch match;
			std::regex compPattern(R"(^(\d+(?:\.\d*)?|\.\d+)[+-](?:\d+(?:\.\d*)?|\.\d+)?i)");
			if (std::regex_search(sub, match, compPattern)) {
				std::string tokenStr = match.str(0);
				tokens.push_back({tokenStr, false});
				i += tokenStr.size();
				canBeUnary = false;
				continue;
			}
			if (isdigit(expr[i]) || expr[i] == '.') {
				std::string num;
				while (i < expr.size() && (isdigit(expr[i]) || expr[i] == '.' || expr[i] == 'i')) {
					num += expr[i++];
				}
				tokens.push_back({num, false});
				canBeUnary = false;
			} else if (isalpha(expr[i])) {
				std::string func;
				while (i < expr.size() && isalpha(expr[i])) {
					func += expr[i++];
				}
				tokens.push_back({func, false});
				canBeUnary = false;
			} else {
				char op = expr[i++];
				if (op == '-' && canBeUnary) {
					tokens.push_back({"_", true});
					canBeUnary = true;
				} else {
					tokens.push_back({std::string(1, op), true});
					canBeUnary = (op == '(' || op == '+' || op == '-' || op == '*' || op == '/' || op == '^');
				}
			}
		}
		return tokens;
	}

	static std::vector<Token> infixToPostfix(const std::vector<Token>& tokens) {
		std::vector<Token> postfix;
		std::stack<Token> opStack;
		for (const auto & token : tokens) {
			if (!token.isOperator && !isFunction(token.value)) {
				postfix.push_back(token);
			} else if (isFunction(token.value)) {
				opStack.push(token);
			} else if (token.value == "(") {
				opStack.push(token);
			} else if (token.value == ")") {
				while (!opStack.empty() && opStack.top().value != "(") {
					postfix.push_back(opStack.top());
					opStack.pop();
				}
				if (!opStack.empty()) {
					opStack.pop();
				}
				if (!opStack.empty() && isFunction(opStack.top().value)) {
					postfix.push_back(opStack.top());
					opStack.pop();
				}
			} else {
				std::string currentOp = token.value;
				while (!opStack.empty()) {
					std::string topValue = opStack.top().value;
					if (topValue == "(") break;
					if (isFunction(topValue)) {
						postfix.push_back(opStack.top());
						opStack.pop();
						continue;
					}
					char topOp = topValue[0];
					char currOpChar = currentOp[0];
					if (currOpChar == '_') {
						if (getPrecedence(topOp) > getPrecedence(currOpChar)) {
							postfix.push_back(opStack.top());
							opStack.pop();
						} else {
							break;
						}
					} else {
						if (getPrecedence(topOp) >= getPrecedence(currOpChar)) {
							postfix.push_back(opStack.top());
							opStack.pop();
						} else {
							break;
						}
					}
				}
				opStack.push(token);
			}
		}
		while (!opStack.empty()) {
			postfix.push_back(opStack.top());
			opStack.pop();
		}
		return postfix;
	}

	static Complex evaluatePostfix(const std::vector<Token>& postfix) {
		std::stack<Complex> valueStack;
		for (const auto & token : postfix) {
			if (isFunction(token.value)) {
				if (valueStack.empty()) throw std::runtime_error("函数 " + token.value + " 缺少参数");
				Complex arg = valueStack.top();
				valueStack.pop();
				valueStack.push(applyFunction(token.value, arg));
			} else if (token.value == "_") {
				if (valueStack.empty()) throw std::runtime_error("负号缺少操作数");
				Complex a = valueStack.top();
				valueStack.pop();
				valueStack.push(-a);
			} else if (token.isOperator) {
				if (valueStack.size() < 2) throw std::runtime_error("运算符 '" + token.value + "' 缺少操作数");
				Complex b = valueStack.top();
				valueStack.pop();
				Complex a = valueStack.top();
				valueStack.pop();
				valueStack.push(applyOperation(a, b, token.value[0]));
			} else {
				if (isNumber(token.value)) {
					valueStack.push(parseComplex(token.value));
				} else {
					throw std::invalid_argument("无效数字: " + token.value);
				}
			}
		}
		return valueStack.empty() ? Complex(0.0Q, 0.0Q) : valueStack.top();
	}
	static Complex applyFunction(std::string_view func, Complex arg) {
		f128 a = arg.real();
		f128 b = arg.imag();
		if (func == "sin") {
			return Complex(sinq(a) * coshq(b), cosq(a) * sinhq(b));
		}
		if (func == "cos") {
			return Complex(cosq(a) * coshq(b), -sinq(a) * sinhq(b));
		}
		if (func == "tan") {
			Complex sz = applyFunction("sin", arg);
			Complex cz = applyFunction("cos", arg);
			if (cz == Complex(0.0Q, 0.0Q)) throw std::runtime_error("错误：正切分母零溢出");
			return sz / cz;
		}
		if (func == "sqrt") {
			f128 r = hypotq(a, b);
			if (r == 0.0Q) return Complex(0.0Q, 0.0Q);
			f128 x, y;
			if (a >= 0.0Q) {
				x = sqrtq((r + a) * 0.5Q);
				y = b / (2.0Q * x);
			} else {
				y = (b >= 0.0Q || (b == 0.0Q && !signbitf(b))) ? sqrtq((r - a) * 0.5Q) : -sqrtq((r - a) * 0.5Q);
				x = b / (2.0Q * y);
			}
			return Complex(x, y);
		}
		if (func == "exp") {
			f128 eA = expq(a);
			return Complex(eA * cosq(b), eA * sinq(b));
		}
		if (func == "ln") {
			f128 r = hypotq(a, b);
			f128 theta = atan2q(b, a);
			return Complex(logq(r), theta);
		}
		if (func == "log" || func == "lg") {
			Complex lnZ = applyFunction("ln", arg);
			return lnZ / logq(10.0Q);
		}
		if (func == "abs") {
			return Complex(hypotq(a, b), 0.0Q);
		}
		throw std::runtime_error("该高精函数暂不支持复数域: " + std::string(func));
	}

	static Complex applyOperation(Complex a, Complex b, char op) {
		switch (op) {
			case '+':
				return a + b;
			case '-':
				return a - b;
			case '*':
				return a * b;
			case '/':
				if (b == Complex(0.0Q, 0.0Q)) throw std::runtime_error("错误：除数不能为零");
				return a / b;
			case '^': {
				if (a == Complex(0.0Q, 0.0Q)) return Complex(0.0Q, 0.0Q);
				Complex lnA = applyFunction("ln", a);
				return applyFunction("exp", b * lnA);
			}
			default:
				throw std::invalid_argument("未知运算符:" + std::string(1, op));
		}
	}

	static bool isFunction(std::string_view token) {
		static const std::unordered_map<std::string_view, bool> functions = {
			{"sin", 1}, {"cos", 1}, {"tan", 1}, {"log", 1}, {"lg", 1}, {"ln", 1},
			{"exp", 1}, {"sqrt", 1}, {"abs", 1}
		};
		return functions.find(token) != functions.end();
	}

	static int getPrecedence(char op) {
		switch (op) {
			case '+':
			case '-':
				return 1;
			case '*':
			case '/':
				return 2;
			case '_':
				return 3;
			case '^':
				return 4;
			default:
				return 0;
		}
	}

	static std::string formatF128(f128 val) {
		if (val == 0.0Q) return "0";
		char buf[256];
		if (fabsq(val) < 1e12Q && fabsq(val) >= 1e-4Q) {
			quadmath_snprintf(buf, sizeof(buf), "%.8Qf", val);
			std::string s(buf);
			s.erase(s.find_last_not_of('0') + 1, std::string::npos);
			if (s.back() == '.') s.pop_back();
			return s;
		} else {
			quadmath_snprintf(buf, sizeof(buf), "%.8Qe", val);
			std::string s(buf);
			size_t ePos = s.find('e');
			if (ePos != std::string::npos) {
				std::string numPart = s.substr(0, ePos);
				std::string expPart = s.substr(ePos);
				numPart.erase(numPart.find_last_not_of('0') + 1, std::string::npos);
				if (numPart.back() == '.') numPart.pop_back();
				return numPart + expPart;
			}
			return s;
		}
	}
	static std::string formatComplex(Complex c) {
		f128 r = c.real();
		f128 i = c.imag();
		if (fabsq(r) < 1e-20Q) r = 0.0Q;
		if (fabsq(i) < 1e-20Q) i = 0.0Q;
		if (i == 0.0Q) return formatF128(r);
		if (r == 0.0Q) {
			if (i == 1.0Q) return "i";
			if (i == -1.0Q) return "-i";
			return formatF128(i) + "i";
		}
		std::string res = formatF128(r);
		if (i > 0.0Q) {
			res += "+";
			if (i == 1.0Q) res += "i";
			else res += formatF128(i) + "i";
		} else {
			if (i == -1.0Q) res += "-i";
			else res += formatF128(i) + "i";
		}
		return res;
	}
	static AngleMode currentAngleMode;
	static Complex memoryValue;
};
calculator::AngleMode calculator::currentAngleMode = calculator::AngleMode::RADIANS;
Complex calculator::memoryValue = Complex(0.0Q, 0.0Q);
std::pair<Color, Color> readcolor() {
	std::ifstream of("set.txt");
	std::string str;
	getline(of, str), getline(of, str);
	Color res;
	bool tag = 0;
	int a = 0, ind = 0;
	int qr[4];
	for (char c : str) {
		if (c == '{') {
			tag = 1;
			continue;
		}
		if (c == '}') {
			tag = 0;
			qr[ind++] = a, a = 0;
			break;
		}
		if (tag && (c != ','))a = a * 10 + c - '0';
		if (c == ',')qr[ind++] = a, a = 0;
	}
	res.r = qr[0];
	res.g = qr[1];
	res.b = qr[2];
	res.a = qr[3];
	Color res2;
	getline(of, str);
	tag = a = 0;
	ind = qr[0] = qr[1] = qr[2] = qr[3] = 0;
	for (char c : str) {
		if (c == '{') {
			tag = 1;
			continue;
		}
		if (c == '}') {
			tag = 0;
			qr[ind++] = a, a = 0;
			continue;
		}
		if (tag && (c != ','))a = a * 10 + c - '0';
		if (c == ',')qr[ind++] = a, a = 0;
	}
	res2.r = qr[0];
	res2.g = qr[1];
	res2.b = qr[2];
	res2.a = qr[3];
	return std::pair<Color, Color>(res, res2);
}
Color readcolor2() {
	std::ifstream of("set.txt");
	std::string str;
	getline(of, str);
	getline(of, str);
	getline(of, str);
	getline(of, str);
	Color res;
	bool tag = 0;
	int a = 0;
	int ind = 0;
	int qr[4];
	for (char c : str) {
		if (c == '{') {
			tag = 1;
			continue;
		}
		if (c == '}') {
			tag = 0;
			qr[ind++] = a, a = 0;
			break;
		}
		if (tag && (c != ','))a = a * 10 + c - '0';
		if (c == ',')qr[ind++] = a, a = 0;
	}
	res.r = qr[0];
	res.g = qr[1];
	res.b = qr[2];
	res.a = qr[3];
	return res;
}
bool inrec(Vector2 mouse, Rectangle rec) {
	return mouse.x >= rec.x && mouse.y >= rec.y && mouse.x <= rec.x + rec.width && mouse.y <= rec.y + rec.height;
}
struct CalcButton {
	std::string label;
	std::string appendText;
};
class read {
	std::string filename;
	bool is_exist(const std::string& filename) {
		return std::filesystem::exists(filename);
	}
public:
	read() {}
	read(const std::string& filenamex) : filename(filenamex) {}

	std::string cc1() {
		if (!is_exist(filename)) {
			return "2675";
		}
		std::ifstream iif(filename);
		std::string text;
		std::getline(iif, text);
		iif.close();
		if (text.length() < 19) return "2675";

		std::string lasttime = text.substr(0, 12); // 截取 "202609172340"
		std::string pt = text.substr(13, 1);       // 截取 "9"

		// 使用位移操作代替 pow
		int gr = 1 << std::stoi(pt);

		// 翻转数字的逻辑
		int prc = 0;
		while (gr) {
			int x = gr % 10;
			prc = prc * 10 + lasttime[x] - '0';
			gr /= 10;
		}

		std::string realp = "";
		for (size_t i = 16; i + 3 <= text.length(); i += 3) {
			int qc = std::stoi(text.substr(i, 3));
			realp.push_back(static_cast<char>(qc ^ prc));
		}

		return realp;
	}
};
class write {
	std::string filename;
	std::string get_current_timestamp() {
		auto now = std::chrono::system_clock::now();
		std::time_t now_time = std::chrono::system_clock::to_time_t(now);
		std::tm* local_tm = std::localtime(&now_time);

		std::ostringstream oss;
		oss << std::put_time(local_tm, "%Y%m%d%H%M");
		return oss.str();
	}

public:
	write() {}
	write(const std::string& filenamex) : filename(filenamex) {}

	bool cc2(const std::string& plain_text) {
		std::ofstream oof(filename);
		if (!oof.is_open()) return false;
		std::string lasttime = get_current_timestamp();
		int q = rand() % 9 + 1;
		int gr = 1 << q;
		std::string pt = std::to_string(q);
		int prc = 0;
		int temp_gr = gr;
		while (temp_gr) {
			int x = temp_gr % 10;
			prc = prc * 10 + (lasttime[x] - '0');
			temp_gr /= 10;
		}
		std::string text = lasttime + "&" + pt + "::";

		// 5. 循环加密每一个字符
		for (char ch : plain_text) {
			int qc = static_cast<int>(ch) ^ prc; // 异或加密
			std::ostringstream oss;
			oss << std::setw(3) << std::setfill('0') << qc;
			text += oss.str();
		}

		// 6. 写入文件
		oof << text;
		oof.close();
		return true;
	}
};
bool AsyncLaunchCalculator(const std::string & commandLine) {
	STARTUPINFOA si;
	PROCESS_INFORMATION pi;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));
	std::vector<char> cmdBuffer(commandLine.begin(), commandLine.end());
	cmdBuffer.push_back('\0');
	char currentExePath[MAX_PATH];
	GetModuleFileNameA(NULL, currentExePath, MAX_PATH);

	std::string cDir(currentExePath);
	size_t lastSlash = cDir.find_last_of("\\/");
	cDir = (lastSlash != std::string::npos) ? cDir.substr(0, lastSlash) : ".";
	if (!CreateProcessA(NULL, cmdBuffer.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, cDir.c_str(), &si, &pi)) return false;
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	return true;
}
struct FontMonoSlider {
	Rectangle bounds;
	float value;
	bool isDragging;
};
struct RGBInputBox {
	Rectangle bounds;
	std::string text;
	bool isFocused;
	int maxValue;
};
Color ParseLineColor(const std::string & str) {
	Color res = { 128, 128, 128, 255 };
	bool tag = false;
	int a = 0, ind = 0;
	int qr[4] = {0};
	for (char c : str) {
		if (c == '{') {
			tag = true;
			continue;
		}
		if (c == '}') {
			tag = false;
			qr[ind++] = a;
			break;
		}
		if (tag && (c != ',')) a = a * 10 + c - '0';
		if (c == ',') {
			if (ind < 4) qr[ind++] = a;
			a = 0;
		}
	}
	if (ind >= 3) {
		res.r = qr[0];
		res.g = qr[1];
		res.b = qr[2];
		res.a = (ind == 4) ? qr[3] : 255;
	}
	return res;
}
std::string FormatColorStr(Color c) {
	return "{" + std::to_string(c.r) + "," + std::to_string(c.g) + "," + std::to_string(c.b) + "," + std::to_string(c.a) + "}";
}
bool isTestPassed(std::string_view expr, std::string_view expected) {
	try {
		std::string rawResult = calculator::calculate(expr);
		if (rawResult == expected) return true;

		auto parseForTest = [](const std::string & s) -> Complex {
			if (s == "i" || s == "+i") return Complex(0.0Q, 1.0Q);
			if (s == "-i") return Complex(0.0Q, -1.0Q);
			size_t ePos = s.find('e');
			std::string base = (ePos == std::string::npos) ? s : s.substr(0, ePos);
			std::string expPart = (ePos == std::string::npos) ? "" : s.substr(ePos);

			if (base.find('i') == std::string::npos) {
				return Complex(strtoflt128(s.c_str(), nullptr), 0.0Q);
			}
			if (base.back() == 'i' && base.find_first_of("+-", 1) == std::string::npos) {
				std::string imStr = base.substr(0, base.size() - 1) + expPart;
				if (imStr.empty() || imStr == "+") return Complex(0.0Q, 1.0Q);
				if (imStr == "-") return Complex(0.0Q, -1.0Q);
				return Complex(0.0Q, strtoflt128(imStr.c_str(), nullptr));
			}
			size_t signPos = base.find_last_of("+-");
			if (signPos != std::string::npos && signPos > 0) {
				std::string realStr = base.substr(0, signPos);
				std::string imagStr = base.substr(signPos, base.size() - signPos - 1) + expPart;
				f128 r = strtoflt128(realStr.c_str(), nullptr);
				f128 i = 0.0Q;
				if (imagStr == "+") i = 1.0Q;
				else if (imagStr == "-") i = -1.0Q;
				else i = strtoflt128(imagStr.c_str(), nullptr);
				return Complex(r, i);
			}
			return Complex(0.0Q, 0.0Q);
		};

		Complex realVal = parseForTest(rawResult);
		Complex expVal = parseForTest(std::string(expected));
		f128 diffR = realVal.real() - expVal.real();
		f128 diffI = realVal.imag() - expVal.imag();
		return hypotq(diffR, diffI) < 1e-4Q;
	} catch (...) {
		return false;
	}
}
std::pair<std::string, std::string> test[34] = {
	{"-abs(-5)", "-5"},
	{"12837-835+937", "12939"},
	{"-5", "-5"},
	{"--5", "5"},
	{"---5", "-5"},
	{"3*-5", "-15"},
	{"3--5", "8"},
	{"5/(-2)", "-2.5"},
	{"abs(-10)", "10"},
	{"sqrt(abs(-16))", "4"},
	{"i", "i"},
	{"-i", "-i"},
	{"--i", "i"},
	{"i*i", "-1"},
	{"(2+3i)+(1-2i)", "3+i"},
	{"(5-2i)-(3+4i)", "2-6i"},
	{"(2+3i)*(1-2i)", "8-i"},
	{"(1+i)*(1-i)", "2"},
	{"(1+2i)/(1+i)", "1.5+0.5i"},
	{"5/(2i)", "-2.5i"},
	{"(3+4i)*-2", "-6-8i"},
	{"abs(-5)*-3i", "-15i"},
	{"sqrt(-4)", "2i"},
	{"sqrt(-1)", "i"},
	{"i^2", "-1"},
	{"i^3", "-i"},
	{"i^4", "1"},
	{"i^i", "0.20787958"},
	{"exp(3.14159265358979323846264338327950288419716939937510*i)", "-1"},
	{"abs(3+4i)", "5"},
	{"-abs(-5i)", "-5"},
	{"sin(i)", "1.17520119i"},
	{"cos(i)", "1.54308063"},
	{"sqrt(-3-4i)", "1-2i"}
};


int main(int argc, char* argv[]) {
	SetTraceLogLevel(LOG_NONE);

	if (argc == 1) {
		if (!is_exist("./password.txt"))make_file("./password.txt");
		if (!is_exist("./set.txt")) {
			make_file("./set.txt");
			std::ofstream of("./set.txt");
			of << "autoupdate:false\ncolorleft:{0,121,241,255}\ncolorright:{135,60,190,255}\ntextcolor:{38,38,38,255}";
		}
		AsyncLaunchCalculator("GUI.exe --password");
		return 0;
	}
	if (argc == 2 && std::string_view(argv[1]) == "--password") {
		InitWindow(800, 300, "输入密码");
		Image favicon = {FAVICON_DATA, FAVICON_WIDTH, FAVICON_HEIGHT, 1, FAVICON_FORMAT};
		SetWindowIcon(favicon);
		SetTargetFPS(60);
		std::string cd = "输入密码: *|错误正确, 将密码错误! 请重新输入在三秒后退出!";
		int coun;
		int* ccd = LoadCodepoints(cd.c_str(), &coun);
		Font ft = LoadFontEx("font.ttf", 50, ccd, coun);
		auto t = readcolor();
		auto c = readcolor2();
		bool startr = 0;
		int frame = 0;
		std::string password;
		std::string realp = "";
		std::ifstream iif("password.txt");
		if (!iif.is_open())realp = "2675";
		else {
			read rd("./password.txt");
			realp = rd.cc1();
			write wr("./password.txt");
			wr.cc2(realp);
		}
		int countname = 0;
		int errorTimer = 0;
		bool shouldExit = false;
		while (!WindowShouldClose()) {
			Vector2 mouse = GetMousePosition();

			// 🌟 核心修正：交互输入与键盘扫描必须在 BeginDrawing 之前完成，杜绝黑屏闪烁与键盘卡死
			if (inrec(mouse, {240, 70, 400, 50}) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
				startr = 1;
			}

			if (errorTimer == 0 && startr) {
				if (IsKeyPressed(KEY_BACKSPACE) && !password.empty()) {
					password.pop_back();
				} else if (IsKeyPressed(KEY_ENTER)) {
					if (password == realp) {
						BeginDrawing();
						DrawRectangleGradientH(0, 0, 800, 300, t.first, t.second);
						DrawTextEx(ft, "密码正确! ", {250, 200}, 50, 1, GREEN);
						EndDrawing();
						AsyncLaunchCalculator("./GUI.exe --calc");
						Sleep(300);
						UnloadFont(ft);
						UnloadCodepoints(ccd);
						CloseWindow();
						std::exit(0);
					} else {
						// 输错逻辑保持不变...
						if (countname >= 2) {
							shouldExit = true;
							errorTimer = 180;
						} else {
							countname++;
							errorTimer = 120;
						}
					}
				} else {
					int key = GetCharPressed();
					while (key > 0) {
						if ((key >= 32) && (key <= 127) && (password.length() < 12)) {
							password += (char)key;
						}
						key = GetCharPressed();
					}
				}
			}

			// 处理倒计时减量
			if (errorTimer > 0) {
				errorTimer--;
				if (errorTimer == 0) {
					if (shouldExit) {
						UnloadFont(ft);
						UnloadCodepoints(ccd);
						CloseWindow();
						std::exit(0);
					}
					password.clear();
				}
			}

			// --- 画面渲染流 ---
			BeginDrawing();
			ClearBackground(RAYWHITE);
			DrawRectangleGradientH(0, 0, 800, 300, t.first, t.second);

			DrawTextEx(ft, "输入密码: ", {55, 70}, 50, 1, c);
			// 绘制优雅的圆角输入密码条白底框
			DrawRectangleRounded({240, 70, 400, 50}, 0.2f, 4, WHITE);

			// 🌟 核心安全修正：使用 std::string 动态拼接星号与呼吸光标，百分之百消灭越界死机与字符重叠 Bug
			std::string starText(password.length(), '*');
			if (startr && errorTimer == 0 && frame < 30) {
				starText += '|'; // 优雅的一秒两次的高精密自适应频率呼吸光标
			}

			// 只有在开始打字且没有报错时才绘制密码条内容
			if (startr && errorTimer == 0) {
				DrawTextEx(ft, starText.c_str(), {255, 70}, 50, 1, c);
			}

			// 优雅绘制报错与死锁警示状态栏
			if (errorTimer > 0) {
				if (shouldExit) {
					DrawTextEx(ft, "密码错误, 将在三秒后退出", {120, 200}, 40, 1, RED);
				} else {
					DrawTextEx(ft, "密码错误! 请重新输入", {220, 200}, 40, 1, RED);
				}
			}

			EndDrawing();
			frame = (frame + 1) % 60;
		}
		UnloadFont(ft);
		UnloadCodepoints(ccd);
		CloseWindow();
		return 0;
	}

// ==================== 分支 2：自适应高级计算器主大厅 ====================
	if (argc == 2 && std::string_view(argv[1]) == "--calc") {
		bool cancalcuatorpass = true;
		for (int i = 0; i <= 33; i++)cancalcuatorpass &= isTestPassed(test[i].first, test[i].second);
		SetConfigFlags(FLAG_WINDOW_RESIZABLE);
		InitWindow(1000, 1000, "高级计算器");
		auto t = readcolor();
		auto c = readcolor2();
		SetTargetFPS(60);
		Image favicon = {FAVICON_DATA, FAVICON_WIDTH, FAVICON_HEIGHT, 1, FAVICON_FORMAT};
		SetWindowIcon(favicon);
		SetWindowMinSize(400, 500);

		std::string cd = "引擎启动失败级参数必须在数[-,]之间函数平方根负号缺少操作数运算符 函数参错误: 发现多余的右括号\')\'错误: 连续的运算符 缺少左操作数错误: 算式不能以运算符结尾错误: 括号未闭合无效数字: 参数必须在 之间平方根参数不能为负数未知函数: 错误：除数不能为零未知运算符:参数不能为负数双轴<滚动条科器! 请输欢括号未知函数: 闭合, 缺少()迎使用科结果: 错误 -算式逻辑非法学计器! 连续式不能以运符的: 缺少左操作数0算式不能为空123456789.abcdefghijklmnopqrstuvwxyz()=+-*/^&|ABCDEFGHIJKLMNOPQRSTUVWXYZ:错误不能为缺少右操作数结尾未闭合包含非法数值未知符号语法-包含非法数值或未知符号\'\"高级计算器请输入式: 结果不能为缺少右结尾未闭合包含非法数值知符号语法'\"复制粘贴全选导出报表";
		int coun;
		int* ccd = LoadCodepoints(cd.c_str(), &coun);
		Font ft = LoadFontEx("font.ttf", 70, ccd, coun);

		std::string currentInput = "";
		std::vector<std::string> historyLines;
		std::ifstream iif("history.his");
		if (iif.eof())historyLines.push_back("欢迎使用计算器! ");
		while (iif.is_open() && !iif.eof()) {
			std::string line;
			std::getline(iif, line);
			if (!line.empty()) historyLines.push_back(line);
		}
		iif.close();
		float cursorTimer = 0.0f;
		bool showCursor = true;
		static float scrollOffsetY = 0.0f;
		static float scrollOffsetX = 0.0f;
		static bool isDraggingBarY = false;
		static float dragStartY = 0.0f;
		static float barStartY = 0.0f;
		static bool isDraggingBarX = false;
		static float dragStartX = 0.0f;
		static float barStartX = 0.0f;

		static bool showContextMenu = false;
		static Vector2 menuPos = { 0, 0 };
		Rectangle copyBtnRec = {0}, pasteBtnRec = {0}, selectAllBtnRec = {0}, exportBtnRec = {0};

		std::vector<CalcButton> buttons = {
			{"sin", "sin("}, {"cos", "cos("}, {"tan", "tan("}, {"^", "^"},   {"AC", "CLEAR"},
			{"7", "7"},     {"8", "8"},     {"9", "9"},     {"/", "/"},   {"(", "("},
			{"4", "4"},     {"5", "5"},     {"6", "6"},     {"*", "*"},   {")", ")"},
			{"1", "1"},     {"2", "2"},     {"3", "3"},     {"-", "-"},   {"sqrt", "sqrt("},
			{"0", "0"},     {".", "."},     {"abs", "abs("}, {"+", "+"},  {"=", "EVAL"}
		};

		auto EvaluateCurrentFormula = [&]() {
			if (currentInput.empty()) return;
			std::string errorLog = "";
			historyLines.push_back("算式: " + currentInput);

			if (!calculator::isValidExpression(currentInput, errorLog)) {
				historyLines.push_back("结果: " + errorLog);
			} else {
				try {
					std::string resStr = calculator::calculate(currentInput);
					historyLines.push_back("结果: " + resStr);
				} catch (const std::exception& e) {
					historyLines.push_back("结果: 错误 - " + std::string(e.what()));
				} catch (const std::domain_error& domain) {
					historyLines.push_back("错误: " + std::string(domain.what()));
				}
			}
			currentInput = "";
		};

		while (!WindowShouldClose()) {
			float W = (float)GetScreenWidth();
			float H = (float)GetScreenHeight();

			Vector2 mousePos = GetMousePosition();
			bool isHistoryChanged = false;
			bool isInputChanged = false;

			Rectangle viewArea = { W * 0.05f, H * 0.12f, W * 0.90f, H * 0.36f };

			float gridStartX = W * 0.05f;
			float gridStartY = H * 0.52f;
			float gridTotalWidth = W * 0.90f;
			float gridTotalHeight = H * 0.33f;


			float spacingX = W * 0.01f;
			float spacingY = H * 0.01f;
			float btnWidth = (gridTotalWidth - (4 * spacingX)) / 5.0f;
			float btnHeight = (gridTotalHeight - (4 * spacingY)) / 4.0f;
			float fontSize = 40.0f * (W / 1000.0f + H / 1000.0f) / 2.0f;
			if (fontSize < 18.0f) fontSize = 18.0f;
			float lineGap = fontSize * 1.15f;
			float fontSpacing = 1.0f;

			int key = GetCharPressed();
			while (key > 0) {
				if ((key >= 32) && (key <= 127) && (currentInput.length() < 500)) {
					currentInput += (char)key;
					isInputChanged = true;
				}
				key = GetCharPressed();
			}
			if (IsKeyPressed(KEY_BACKSPACE) && !currentInput.empty()) {
				currentInput.pop_back();
				isInputChanged = true;
			}

			if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
				if (IsKeyPressed(KEY_V)) {
					const char* clipText = GetClipboardText();
					if (clipText != nullptr) {
						for (char ch : std::string(clipText)) {
							if (currentInput.length() >= 500) break;
							if (ch >= 32 && ch <= 127) {
								currentInput += ch;
								isInputChanged = true;
							}
						}
					}
				}
				if (IsKeyPressed(KEY_C) && !currentInput.empty()) {
					SetClipboardText(currentInput.c_str());
				}
			}

			if (IsKeyPressed(KEY_ENTER)) {
				EvaluateCurrentFormula();
				isHistoryChanged = true;
			}

			for (int i = 0; i < buttons.size(); i++) {
				int row = i / 5;
				int col = i % 5;
				Rectangle btnRec = { gridStartX + col* (btnWidth + spacingX), gridStartY + row* (btnHeight + spacingY), btnWidth, btnHeight };

				if (row == 2 && col == 4 && buttons[i].label.empty()) buttons[i].label = "<-";

				if (inrec(mousePos, btnRec) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !showContextMenu) {
					if (buttons[i].label == "<-") {
						if (!currentInput.empty()) {
							currentInput.pop_back();
							isInputChanged = true;
						}
					} else if (buttons[i].appendText == "CLEAR") {
						currentInput = "";
						isInputChanged = true;
					} else if (buttons[i].appendText == "EVAL") {
						EvaluateCurrentFormula();
						isHistoryChanged = true;
					} else {
						if (currentInput.length() < 500) {
							currentInput += buttons[i].appendText;
							isInputChanged = true;
						}
					}
				}
			}

			if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
				showContextMenu = true;
				menuPos = mousePos;
			}
			if (showContextMenu) {
				float mWidth = fontSize * 3.5f;
				float mHeight = fontSize * 0.9f;
				copyBtnRec      = { menuPos.x, menuPos.y, mWidth, mHeight };
				pasteBtnRec     = { menuPos.x, menuPos.y + mHeight, mWidth, mHeight };
				selectAllBtnRec = { menuPos.x, menuPos.y + mHeight * 2, mWidth, mHeight };
				exportBtnRec    = { menuPos.x, menuPos.y + mHeight * 3, mWidth, mHeight };

				if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !inrec(mousePos, copyBtnRec) && !inrec(mousePos, pasteBtnRec) && !inrec(mousePos, selectAllBtnRec) && !inrec(mousePos, exportBtnRec)) {
					showContextMenu = false;
				}
				if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && inrec(mousePos, copyBtnRec)) {
					if (!currentInput.empty()) SetClipboardText(currentInput.c_str());
					showContextMenu = false;
				}
				if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && inrec(mousePos, pasteBtnRec)) {
					const char* clip = GetClipboardText();
					if (clip) {
						for (char ch : std::string(clip)) {
							if (ch >= 32 && ch <= 127) currentInput += ch;
						}
						isInputChanged = true;
					}
					showContextMenu = false;
				}
				if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && inrec(mousePos, selectAllBtnRec)) {
					currentInput = "";
					isInputChanged = true;
					showContextMenu = false;
				}
				if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && inrec(mousePos, exportBtnRec)) {
					std::ofstream html("history_report.html");
					if (html.is_open()) {
						html << "<!DOCTYPE html>\n<html>\n<head><meta charset=\"UTF-8\"><title>历史报表</title>\n";
						html << "<style>:root { --theme: rgb(" << (int)c.r << "," << (int)c.g << "," << (int)c.b << "); }\n";
						html << "body { font-family: sans-serif; background: linear-gradient(135deg, rgb(" << (int)t.first.r << "," << (int)t.first.g << "," << (int)t.first.b << "), rgb(" << (int)t.second.r << "," << (int)t.second.g << "," << (int)t.second.b << ")); min-height: 100vh; display: flex; justify-content: center; align-items: center; margin: 0; }\n";
						html << ".card { background: rgba(255,255,255,0.85); backdrop-filter: blur(20px); border-radius: 12px; padding: 30px; width: 500px; box-shadow: 0 15px 30px rgba(0,0,0,0.2); }\n";
						html << ".row { padding: 10px; font-family: monospace; font-size: 16px; border-left: 4px solid var(--theme); margin: 6px 0; }\n";
						html << ".res { border-left-color: gold; background: rgba(255,215,0,0.1); font-weight: bold; }\n";
						html << "</style></head><body><div class=\"card\"><h2>🧮 历史计算流体纸带</h2>";
						for (const auto& line : historyLines) {
							std::string cls = (line.rfind("结果:", 0) == 0) ? "row res" : "row";
							html << "<div class=\"" << cls << "\">" << line << "</div>";
						}
						html << "</div></body></html>";
						html.close();
						isHistoryChanged = true;
					}
					showContextMenu = false;
				}
			}

			float wheelMove = GetMouseWheelMove();
			if (wheelMove != 0.0f) scrollOffsetY += wheelMove * lineGap;

			Rectangle vBarArea = { viewArea.x + viewArea.width + spacingX * 0.5f, viewArea.y, 8.0f, viewArea.height };
			Rectangle hBarArea = { viewArea.x, viewArea.y + viewArea.height + spacingY * 0.5f, viewArea.width, 8.0f };

			std::string renderInputText = "请输入算式: " + currentInput;
			Vector2 currentInputSize = MeasureTextEx(ft, renderInputText.c_str(), fontSize, fontSpacing);

			float totalContentHeight = (historyLines.size() + 1) * lineGap;
			bool isLongContentY = totalContentHeight > viewArea.height;
			float maxScrollY = viewArea.height - totalContentHeight;
			if (isLongContentY) {
				if (isHistoryChanged) scrollOffsetY = maxScrollY;
				if (scrollOffsetY > 0.0f) scrollOffsetY = 0.0f;
				if (scrollOffsetY < maxScrollY) scrollOffsetY = maxScrollY;
			} else {
				scrollOffsetY = 0.0f;
				isDraggingBarY = false;
			}

			float maxTextWidthFound = 0.0f;
			for (const auto& line : historyLines) {
				Vector2 size = MeasureTextEx(ft, line.c_str(), fontSize, fontSpacing);
				if (size.x > maxTextWidthFound) maxTextWidthFound = size.x;
			}
			if (currentInputSize.x + 25.0f > maxTextWidthFound) maxTextWidthFound = currentInputSize.x + 25.0f;
			bool isLongContentX = maxTextWidthFound > viewArea.width;
			float maxScrollX = viewArea.width - maxTextWidthFound;
			if (isLongContentX) {
				if (!isDraggingBarX && (isInputChanged || isHistoryChanged)) scrollOffsetX = maxScrollX;
				if (scrollOffsetX > 0.0f) scrollOffsetX = 0.0f;
				if (scrollOffsetX < maxScrollX) scrollOffsetX = maxScrollX;
			} else {
				scrollOffsetX = 0.0f;
				isDraggingBarX = false;
			}
			Rectangle hHandleRect = { 0 };
			if (isLongContentX) {
				float hWidth = (viewArea.width / maxTextWidthFound) * hBarArea.width;
				if (hWidth < 30.0f) hWidth = 30.0f;
				float scrollProgressX = (maxScrollX == 0.0f) ? 0.0f : (scrollOffsetX / maxScrollX);
				hHandleRect = { hBarArea.x + scrollProgressX* (hBarArea.width - hWidth), hBarArea.y, hWidth, hBarArea.height };
				if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && inrec(mousePos, hHandleRect) && !showContextMenu) {
					isDraggingBarX = true;
					dragStartX = mousePos.x;
					barStartX = hHandleRect.x;
				}
				if (isDraggingBarX && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
					float deltaX = mousePos.x - dragStartX;
					float nX = barStartX + deltaX;
					if (nX < hBarArea.x) nX = hBarArea.x;
					if (nX > hBarArea.x + hBarArea.width - hHandleRect.width) nX = hBarArea.x + hBarArea.width - hHandleRect.width;
					scrollOffsetX = (nX - hBarArea.x) / (hBarArea.width - hHandleRect.width) * maxScrollX;
				} else {
					isDraggingBarX = false;
				}
			}
			Rectangle vHandleRect = { 0 };
			if (isLongContentY) {
				float hHeight = (viewArea.height / totalContentHeight) * vBarArea.height;
				if (hHeight < 30.0f) hHeight = 30.0f;
				float scrollProgressY = (maxScrollY == 0.0f) ? 0.0f : (scrollOffsetY / maxScrollY);
				vHandleRect = { vBarArea.x, vBarArea.y + scrollProgressY* (vBarArea.height - hHeight), vBarArea.width, hHeight };
				if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && inrec(mousePos, vHandleRect) && !showContextMenu) {
					isDraggingBarY = true;
					dragStartY = mousePos.y;
					barStartY = vHandleRect.y;
				}
				if (isDraggingBarY && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
					float deltaY = mousePos.y - dragStartY;
					float nY = barStartY + deltaY;
					if (nY < vBarArea.y) nY = vBarArea.y;
					if (nY > vBarArea.y + vBarArea.height - vHandleRect.height) nY = vBarArea.y + vBarArea.height - vHandleRect.height;
					scrollOffsetY = (nY - vBarArea.y) / (vBarArea.height - vHandleRect.height) * maxScrollY;
				} else {
					isDraggingBarY = false;
				}
			}
			cursorTimer += GetFrameTime();
			if (cursorTimer >= 0.5f) {
				showCursor = !showCursor;
				cursorTimer = 0.0f;
			}
			BeginDrawing();
			ClearBackground(RAYWHITE);
			DrawRectangleGradientH(0, 0, (int)W, (int)H, t.first, t.second);
			if (!cancalcuatorpass) DrawTextEx(ft, "计算引擎启动失败", { viewArea.x, viewArea.y - fontSize }, fontSize * 0.5f, 1, RED);
			Vector2 titleSize = MeasureTextEx(ft, "计算器", fontSize * 1.5f, 1);
			DrawTextEx(ft, "计算器", { W * 0.5f - titleSize.x * 0.5f, H * 0.03f }, fontSize * 1.5f, 1, c);
			Vector2 gearCenter = {GetScreenWidth() - 35.0f, 0.0f + 35.0f };
			Color gearGray = { 125, 140, 155, 255 };
			Color innerWhite = { 255, 255, 255, 255 };
			Color centerBlue = { 0, 102, 204, 255 };
			static float currentAngle = 0.0f;
			if (inrec(mousePos, {GetScreenWidth() - 60.0f, 10.0f, 50, 50})) {
				currentAngle += 1.5f;
				if (currentAngle >= 360.0f) currentAngle -= 360.0f;
			}
			for (int i = 0; i < 6; i++) {
				float r = (currentAngle + i * 60.0f) * DEG2RAD;
				DrawCircleV({gearCenter.x + 17.5f * cosf(r), gearCenter.y + 17.5f * sinf(r)}, 7.0f, gearGray);
			}
			DrawCircleV(gearCenter, 18.0f, gearGray);
			DrawCircleV(gearCenter, 12.0f, innerWhite);
			DrawCircleV(gearCenter, 7.5f, centerBlue);
			if (inrec(mousePos, {GetScreenWidth() - 60.0f, 10.0f, 50, 50}) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !showContextMenu) AsyncLaunchCalculator("GUI.exe --settings");
			BeginScissorMode((int)viewArea.x, (int)viewArea.y, (int)viewArea.width, (int)viewArea.height);
			float cLY = viewArea.y + scrollOffsetY;
			for (const auto& line : historyLines) {
				if (cLY >= viewArea.y - lineGap && cLY <= viewArea.y + viewArea.height) {
					DrawTextEx(ft, line.c_str(), { viewArea.x + scrollOffsetX, cLY }, fontSize, fontSpacing, (line.rfind("结果:", 0) == 0) ? YELLOW : c);
				}
				cLY += lineGap;
			}
			if (cLY >= viewArea.y - lineGap && cLY <= viewArea.y + viewArea.height) {
				DrawTextEx(ft, renderInputText.c_str(), { viewArea.x + scrollOffsetX, cLY }, fontSize, fontSpacing, c);
				if (showCursor) DrawTextEx(ft, "|", { viewArea.x + scrollOffsetX + currentInputSize.x + 2.0f, cLY + 2.0f }, fontSize, fontSpacing, c);
			}
			EndScissorMode();
			if (isLongContentY) {
				DrawRectangleRec(vBarArea, { c.r, c.g, c.b, 30 });
				DrawRectangleRounded(vHandleRect, 0.5f, 4, inrec(mousePos, vHandleRect) ? c : Color{ c.r, c.g, c.b, 150 });
			}
			if (isLongContentX) {
				DrawRectangleRec(hBarArea, { c.r, c.g, c.b, 30 });
				DrawRectangleRounded(hHandleRect, 0.5f, 4, inrec(mousePos, hHandleRect) ? c : Color{ c.r, c.g, c.b, 150 });
			}
			DrawRectangleLinesEx({ viewArea.x - 5, viewArea.y - 5, viewArea.width + 10, viewArea.height + 10 }, 1, { c.r, c.g, c.b, 50 });
			for (int i = 0; i < buttons.size(); i++) {
				int row = i / 5;
				int col = i % 5;
				Rectangle bRec = { gridStartX + col* (btnWidth + spacingX), gridStartY + row* (btnHeight + spacingY), btnWidth, btnHeight };
				bool isH = inrec(mousePos, bRec);
				bool isD = isH && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
				Color bCol = (buttons[i].label == "AC") ? (isD ? MAROON : (isH ? RED : Color{230, 60, 60, 220})) : ((buttons[i].label == "=") ? (isD ? GOLD : (isH ? YELLOW : Color{255, 200, 0, 220})) : (isD ? c : (isH ? Color{c.r, c.g, c.b, 70} : Color{c.r, c.g, c.b, 30})));
				DrawRectangleRounded(bRec, 0.2f, 4, bCol);
				DrawRectangleRoundedLines(bRec, 0.2f, 4, { c.r, c.g, c.b, 80 });
				float currentBtnFontSize = fontSize * 0.75f;
				Vector2 lSz = MeasureTextEx(ft, buttons[i].label.c_str(), currentBtnFontSize, 1);
				DrawTextEx(ft, buttons[i].label.c_str(), { bRec.x + (bRec.width - lSz.x) / 2.0f, bRec.y + (bRec.height - lSz.y) / 2.0f }, currentBtnFontSize, 1, c);
			}
			if (showContextMenu) {
				float mH = copyBtnRec.height;
				DrawRectangle(copyBtnRec.x + 3, copyBtnRec.y + 3, copyBtnRec.width, mH * 4, Color{ 0, 0, 0, 50 });
				DrawRectangle(copyBtnRec.x, copyBtnRec.y, copyBtnRec.width, mH * 4, Color{ t.second.r, t.second.g, t.second.b, 245 });
				DrawRectangleLinesEx({ copyBtnRec.x, copyBtnRec.y, copyBtnRec.width, mH * 4 }, 1, { 200, 200, 200, 100 });
				Rectangle items[] = { copyBtnRec, pasteBtnRec, selectAllBtnRec, exportBtnRec };
				std::string labels[] = { "复制", "粘贴", "全选", "导出报表" };
				for (int i = 0; i < 4; i++) {
					bool isHoverItem = inrec(mousePos, items[i]);
					if (isHoverItem) DrawRectangleRec(items[i], Color{ c.r, c.g, c.b, 60 });
					DrawTextEx(ft, labels[i].c_str(), { items[i].x + 15.0f, items[i].y + (mH - fontSize * 0.6f) / 2.0f }, fontSize * 0.6f, 1, isHoverItem ? WHITE : c);
				}
			}
			EndDrawing();
		}
		std::ofstream oof("history.his");
		for (const auto& line : historyLines) oof << line << "\n";
		UnloadFont(ft);
		UnloadCodepoints(ccd);
		CloseWindow();
		return 0;
	}
	if (argc == 2 && std::string_view(argv[1]) == "--settings") {
		InitWindow(1000, 1000, "主题配置调色盘");
		SetTargetFPS(60);
		Image favicon = {FAVICON_DATA, FAVICON_WIDTH, FAVICON_HEIGHT, 1, FAVICON_FORMAT};
		SetWindowIcon(favicon);
		Color bgStart = {30, 30, 45, 255};
		Color bgEnd = {70, 40, 80, 255};
		Color fontColor = {255, 255, 255, 255};
		Color ff;
		Color bgs, bge;
		std::ifstream inFile("set.txt");
		if (inFile.is_open()) {
			std::string line0, line1, line2, line3, line4;
			getline(inFile, line0);
			getline(inFile, line1);
			getline(inFile, line2);
			getline(inFile, line3);
			getline(inFile, line4);
			bgStart = ParseLineColor(line1);
			bgEnd = ParseLineColor(line2);
			fontColor = ParseLineColor(line3);
			inFile.close();
		}
		bgs = bgStart, bge = bgEnd, ff = fontColor;
		std::string cd = "高级主题配置大厅调CGILTU字体颜色默认启动: 一维! 滑动条 (0 - 255):色盘左渐变右输入字体颜计算器预览: 1+2+3=6色单轴滑动条保存设置成功失败RB0123456789:{}";
		int coun;
		int* ccd = LoadCodepoints(cd.c_str(), &coun);
		Font ft = LoadFontEx("font.ttf", 50, ccd, coun);
		std::vector<RGBInputBox> rgbBoxes = {
			{{ 250, 160, 90, 40 }, std::to_string(bgStart.r), false, 255},
			{{ 350, 160, 90, 40 }, std::to_string(bgStart.g), false, 255},
			{{ 450, 160, 90, 40 }, std::to_string(bgStart.b), false, 255},
			{{ 250, 260, 90, 40 }, std::to_string(bgEnd.r), false, 255},
			{{ 350, 260, 90, 40 }, std::to_string(bgEnd.g), false, 255},
			{{ 450, 260, 90, 40 }, std::to_string(bgEnd.b), false, 255}
		};
		float initVal = ((fontColor.r + fontColor.g + fontColor.b) / 3.0f) / 255.0f;
		FontMonoSlider monoSlider = { { 180, 390, 380, 20 }, initVal, false };
		Rectangle saveBtn = { 360, 830, 280, 60 };
		std::string toastText = "";
		float toastTimer = 0.0f;
		while (!WindowShouldClose()) {
			Vector2 mousePos = GetMousePosition();
			if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
				for (auto& box : rgbBoxes) {
					box.isFocused = inrec(mousePos, box.bounds);
				}
			}
			for (auto& box : rgbBoxes) {
				if (box.isFocused) {
					int key = GetCharPressed();
					while (key > 0) {
						if (key >= '0' && key <= '9' && box.text.length() < 3) {
							box.text += (char)key;
							// 越界值安全截断保护
							if (std::stoi(box.text) > box.maxValue) box.text = std::to_string(box.maxValue);
						}
						key = GetCharPressed();
					}
					if (IsKeyPressed(KEY_BACKSPACE) && !box.text.empty()) {
						box.text.pop_back();
					}
				}
			}

			// 🌟 交互逻辑 B：单轴字体滑动条状态检测与一维强行对齐
			if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && inrec(mousePos, monoSlider.bounds)) {
				monoSlider.isDragging = true;
			}
			if (monoSlider.isDragging) {
				if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
					monoSlider.value = (mousePos.x - monoSlider.bounds.x) / monoSlider.bounds.width;
					if (monoSlider.value < 0.0f) monoSlider.value = 0.0f;
					if (monoSlider.value > 1.0f) monoSlider.value = 1.0f;
				} else {
					monoSlider.isDragging = false;
				}
			}
			int rStart = rgbBoxes[0].text.empty() ? 0 : std::stoi(rgbBoxes[0].text);
			int gStart = rgbBoxes[1].text.empty() ? 0 : std::stoi(rgbBoxes[1].text);
			int bStart = rgbBoxes[2].text.empty() ? 0 : std::stoi(rgbBoxes[2].text);
			bgStart = { (unsigned char)rStart, (unsigned char)gStart, (unsigned char)bStart, 255 };

			int rEnd = rgbBoxes[3].text.empty() ? 0 : std::stoi(rgbBoxes[3].text);
			int gEnd = rgbBoxes[4].text.empty() ? 0 : std::stoi(rgbBoxes[4].text);
			int bEnd = rgbBoxes[5].text.empty() ? 0 : std::stoi(rgbBoxes[5].text);
			bgEnd = { (unsigned char)rEnd, (unsigned char)gEnd, (unsigned char)bEnd, 255 };
			unsigned char monoRGBValue = (unsigned char)(monoSlider.value * 255);
			fontColor = { monoRGBValue, monoRGBValue, monoRGBValue, 255 };
			if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && inrec(mousePos, saveBtn)) {
				std::ofstream outFile("set.txt");
				if (outFile.is_open()) {
					outFile << "autoupdate:false\n";
					outFile << "colorleft:";
					outFile << FormatColorStr(bgStart) << std::endl;
					outFile << "colorright:";
					outFile << FormatColorStr(bgEnd) << std::endl;
					outFile << "textcolor:";
					outFile << FormatColorStr(fontColor) << std::endl;
					outFile.close();
					toastText = "保存成功! ";
					toastTimer = 2.0f;
				} else {
					toastText = "保存失败! ";
					toastTimer = 2.0f;
				}
			}

			if (toastTimer > 0.0f) toastTimer -= GetFrameTime();

			// --- 画面渲染周期 (Draw) ---
			BeginDrawing();
			ClearBackground(GRAY);
			DrawRectangleGradientH(0, 0, 1000, 1000, bgs, bge);
			DrawRectangle(30, 20, 940, 100, {200, 200, 40, 40});
			DrawRectangleGradientH(35, 25, 930, 90, bgStart, bgEnd); // 预览实时渐变背景
			// 预览字体渲染（字体颜色被单轴滑块强行控制）
			DrawTextEx(ft, "计算器预览: 1+2+3=6", { 60, 45 }, 30, 1, fontColor);

			// 2. 下半部：高级极客调色控制台
			DrawLineEx({ 30, 140 }, { 970, 140 }, 1, LIGHTGRAY);

			// 绘制输入说明文本
			DrawTextEx(ft, "左渐变颜色输入: ", { 30, 165 }, 30, 1, ff);
			DrawTextEx(ft, "右渐变颜色输入: ", { 30, 265 }, 30, 1, ff);
			DrawTextEx(ft, "字体颜色一维滑动条 (0 - 255):", { 30, 350 }, 30, 1, ff);

			// 绘制 6 组输入框
			for (const auto& box : rgbBoxes) {
				DrawRectangleRec(box.bounds, box.isFocused ? WHITE : DARKGRAY);
				DrawRectangleLinesEx(box.bounds, 1, box.isFocused ? GREEN : LIGHTGRAY);
				if (!box.text.empty()) {
					DrawTextEx(ft, box.text.c_str(), { box.bounds.x + 10, box.bounds.y + 8 }, 24, 1, box.isFocused ? BLACK : WHITE);
				}
			}

			// 绘制一维字体单轴滑动条轨道与纽扣
			DrawRectangleRec(monoSlider.bounds, DARKGRAY);
			float fillWidth = monoSlider.bounds.width * monoSlider.value;
			DrawRectangle(monoSlider.bounds.x, monoSlider.bounds.y, fillWidth, monoSlider.bounds.height, LIGHTGRAY);
			Vector2 knob = { monoSlider.bounds.x + fillWidth, monoSlider.bounds.y + monoSlider.bounds.height / 2.0f };
			DrawCircleV(knob, 12.0f, WHITE);
			DrawCircleV(knob, 6.0f, DARKGRAY);

			std::string monoValueLabel = std::to_string(monoRGBValue);
			DrawTextEx(ft, monoValueLabel.c_str(), { monoSlider.bounds.x + monoSlider.bounds.width + 15, monoSlider.bounds.y - 4 }, 30, 1, ff);
			bool hoverBtn = inrec(mousePos, saveBtn);
			DrawRectangleRec(saveBtn, hoverBtn ? WHITE : LIGHTGRAY);
			DrawTextEx(ft, "保存设置", { saveBtn.x + 85, saveBtn.y + 15 }, 30, 1, ff);
			std::string ttrue = "保存成功! ", ttfalse = "保存失败! ";
			Color ttcolor = ff;
			for (int i = 0; i < ttrue.length(); i++) {
				if (ttrue[i] != toastText[i])break;
				if (i == ttrue.length() - 1)ttcolor = GREEN;
			}
			for (int i = 0; i < ttfalse.length(); i++) {
				if (ttfalse[i] != toastText[i])break;
				if (i == ttfalse.length() - 1)ttcolor = RED;
			}
			if (toastTimer > 0.0f) DrawTextEx(ft, toastText.c_str(), { 390, 900}, 50, 1, ttcolor);
			EndDrawing();
		}
		UnloadFont(ft);
		UnloadCodepoints(ccd);
		CloseWindow();
		return 0;
	}
	return 0;
}
