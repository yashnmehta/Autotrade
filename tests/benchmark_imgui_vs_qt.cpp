/*
 * Benchmark: ImGui vs Qt Table Update Performance
 * 
 * Compares rendering performance for real-time market data updates:
 * 1. Qt QTableView with QAbstractTableModel (emit dataChanged)
 * 2. ImGui Table API (direct rendering)
 * 
 * Simulates high-frequency market data updates (100-1000 ticks/sec)
 */

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <numeric>
#include <algorithm>
#include <cstring>

// ============================================================================
// CONFIGURATION
// ============================================================================

constexpr int NUM_ROWS = 50;
constexpr int NUM_COLS = 10;
constexpr int TEST_DURATION_SEC = 10;

// ============================================================================
// MARKET DATA MODEL
// ============================================================================

struct MarketDataTable {
    std::vector<std::vector<double>> data;
    std::mt19937 rng;
    std::uniform_real_distribution<double> dist;
    std::uniform_int_distribution<int> rowDist;
    std::uniform_int_distribution<int> colDist;
    
    // Statistics
    int updateCount = 0;
    uint64_t totalLatencyNs = 0;
    int frameCount = 0;
    uint64_t totalFrameTimeUs = 0;
    
    // Highlight state
    std::vector<std::vector<int>> directions; // 1 for up, -1 for down
    std::vector<std::vector<int>> highlightFrames; // Remaining frames to highlight
    
    MarketDataTable() 
        : dist(100.0, 500.0)
        , rowDist(0, NUM_ROWS - 1)
        , colDist(0, NUM_COLS - 1)
    {
        std::random_device rd;
        rng.seed(rd());
        
        // Initialize data and highlights
        data.resize(NUM_ROWS);
        directions.resize(NUM_ROWS);
        highlightFrames.resize(NUM_ROWS);
        for (int i = 0; i < NUM_ROWS; ++i) {
            data[i].resize(NUM_COLS);
            directions[i].resize(NUM_COLS, 0);
            highlightFrames[i].resize(NUM_COLS, 0);
            for (int j = 0; j < NUM_COLS; ++j) {
                data[i][j] = dist(rng);
            }
        }
    }
    
    void updateRandomCell() {
        auto t_start = std::chrono::high_resolution_clock::now();
        
        int row = rowDist(rng);
        int col = colDist(rng);
        
        double change = dist(rng) * 0.01; // ±1% change
        bool up = (rng() % 2 == 0);
        data[row][col] += (up ? change : -change);
        
        // Update highlight
        directions[row][col] = (up ? 1 : -1);
        highlightFrames[row][col] = 10; // Highlight for 10 frames
        
        auto t_end = std::chrono::high_resolution_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(t_end - t_start).count();
        
        updateCount++;
        totalLatencyNs += latency;
    }
    
    void updateRandomRow() {
        auto t_start = std::chrono::high_resolution_clock::now();
        
        int row = rowDist(rng);
        
        for (int col = 0; col < NUM_COLS; ++col) {
            double change = dist(rng) * 0.01;
            bool up = (rng() % 2 == 0);
            data[row][col] += (up ? change : -change);
            
            directions[row][col] = (up ? 1 : -1);
            highlightFrames[row][col] = 10;
        }
        
        auto t_end = std::chrono::high_resolution_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(t_end - t_start).count();
        
        updateCount++;
        totalLatencyNs += latency;
    }
    
    void recordFrame(uint64_t frameTimeUs) {
        frameCount++;
        totalFrameTimeUs += frameTimeUs;
    }
    
    double getAverageLatencyNs() const {
        return updateCount > 0 ? static_cast<double>(totalLatencyNs) / updateCount : 0.0;
    }
    
    double getAverageFrameTimeUs() const {
        return frameCount > 0 ? static_cast<double>(totalFrameTimeUs) / frameCount : 0.0;
    }
    
    void resetStats() {
        updateCount = 0;
        totalLatencyNs = 0;
        frameCount = 0;
        totalFrameTimeUs = 0;
        for (int i = 0; i < NUM_ROWS; ++i) {
            std::fill(highlightFrames[i].begin(), highlightFrames[i].end(), 0);
        }
    }
};

// ============================================================================
// BENCHMARK STATE
// ============================================================================

struct BenchmarkState {
    MarketDataTable table;
    
    // Test configuration
    int updateFrequency = 100; // updates per second
    int updateType = 1; // 0 = single cell, 1 = full row
    bool testRunning = false;
    
    // Timing
    std::chrono::high_resolution_clock::time_point lastUpdateTime;
    std::chrono::high_resolution_clock::time_point testStartTime;
    double updateInterval = 0.01; // seconds between updates
    
    // Results
    char resultsText[2048] = "Click 'Start Test' to begin...";
    char statsText[256] = "Ready";
    
    BenchmarkState() {
        updateInterval = 1.0 / updateFrequency;
        lastUpdateTime = std::chrono::high_resolution_clock::now();
        startTest(); // Start automatically for headless testing
    }
    
    void startTest() {
        testRunning = true;
        table.resetStats();
        testStartTime = std::chrono::high_resolution_clock::now();
        lastUpdateTime = std::chrono::high_resolution_clock::now();
        updateInterval = 1.0 / updateFrequency;
    }
    
    void stopTest() {
        testRunning = false;
        
        auto t_end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            t_end - testStartTime).count();
        
        int updates = table.updateCount;
        double modelLatency = table.getAverageLatencyNs();
        int frames = table.frameCount;
        double frameTime = table.getAverageFrameTimeUs();
        
        double actualFPS = (frames * 1000.0) / duration;
        double updateRate = (updates * 1000.0) / duration;
        
        snprintf(resultsText, sizeof(resultsText),
            "========================================\n"
            "TEST RESULTS: ImGui Direct Rendering\n"
            "========================================\n"
            "Duration:         %lld ms\n"
            "Total updates:    %d\n"
            "Update rate:      %.1f updates/sec\n"
            "Model latency:    %.2f µs (avg)\n"
            "Frame count:      %d\n"
            "Frame time:       %.2f µs (avg)\n"
            "Actual FPS:       %.1f\n"
            "========================================\n",
            static_cast<long long>(duration), updates, updateRate, 
            modelLatency / 1000.0, frames, frameTime, actualFPS);
        
        std::cout << resultsText << std::endl;
    }
    
    void update() {
        if (!testRunning) return;
        
        auto now = std::chrono::high_resolution_clock::now();
        
        // Check if test duration exceeded
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - testStartTime).count();
        if (elapsed >= TEST_DURATION_SEC) {
            stopTest();
            return;
        }
        
        // Check if it's time for an update
        double timeSinceUpdate = std::chrono::duration<double>(
            now - lastUpdateTime).count();
        
        if (timeSinceUpdate >= updateInterval) {
            if (updateType == 0) {
                table.updateRandomCell();
            } else {
                table.updateRandomRow();
            }
            lastUpdateTime = now;
        }
        
        // Update stats display
        int updates = table.updateCount;
        double modelLatency = table.getAverageLatencyNs();
        int frames = table.frameCount;
        double frameTime = table.getAverageFrameTimeUs();
        
        snprintf(statsText, sizeof(statsText),
            "Updates: %d | Model: %.2f µs | Frames: %d | Frame: %.2f µs",
            updates, modelLatency / 1000.0, frames, frameTime);
    }
};

// ============================================================================
// IMGUI RENDERING
// ============================================================================

void RenderBenchmarkUI(BenchmarkState& state) {
    ImGui::Begin("ImGui Table Update Benchmark", nullptr, ImGuiWindowFlags_NoCollapse);
    
    ImGui::Text("Model Update Performance Comparison");
    ImGui::Separator();
    
    // Controls
    ImGui::Text("Configuration:");
    
    const char* frequencies[] = { "10", "50", "100", "200", "500", "1000" };
    static int freqIdx = 2; // Default to 100
    if (ImGui::Combo("Frequency (updates/sec)", &freqIdx, frequencies, IM_ARRAYSIZE(frequencies))) {
        state.updateFrequency = std::stoi(frequencies[freqIdx]);
        state.updateInterval = 1.0 / state.updateFrequency;
    }
    
    const char* updateTypes[] = { "Single Cell", "Full Row" };
    ImGui::Combo("Update Type", &state.updateType, updateTypes, IM_ARRAYSIZE(updateTypes));
    
    ImGui::Spacing();
    
    if (!state.testRunning) {
        if (ImGui::Button("Start Test", ImVec2(120, 30))) {
            state.startTest();
        }
    } else {
        if (ImGui::Button("Stop Test", ImVec2(120, 30))) {
            state.stopTest();
        }
    }
    
    ImGui::SameLine();
    ImGui::TextColored(state.testRunning ? ImVec4(0, 1, 0, 1) : ImVec4(0.5, 0.5, 0.5, 1), 
                       state.testRunning ? "RUNNING" : "IDLE");
    
    ImGui::Separator();
    
    // Stats display
    ImGui::TextColored(ImVec4(0, 0.8f, 1, 1), "%s", state.statsText);
    
    ImGui::Separator();
    
    // Market data table
    ImGui::Text("Market Data Table (%d x %d)", NUM_ROWS, NUM_COLS);
    
    auto frameStart = std::chrono::high_resolution_clock::now();
    
    static ImGuiTableFlags flags = 
        ImGuiTableFlags_Borders | 
        ImGuiTableFlags_RowBg | 
        ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingFixedFit;
    
    if (ImGui::BeginTable("MarketDataTable", NUM_COLS, flags, ImVec2(0, 400))) {
        // Headers
        static const char* headers[] = {
            "Symbol", "LTP", "Bid", "Ask", "Volume", "High", "Low", "Open", "Close", "Change"
        };
        
        for (int col = 0; col < NUM_COLS; ++col) {
            ImGui::TableSetupColumn(headers[col], ImGuiTableColumnFlags_WidthFixed, 80.0f);
        }
        ImGui::TableHeadersRow();
        
        // Data rows
        for (int row = 0; row < NUM_ROWS; ++row) {
            ImGui::TableNextRow();
            
            for (int col = 0; col < NUM_COLS; ++col) {
                ImGui::TableSetColumnIndex(col);
                
                double value = state.table.data[row][col];
                
                // Cell background color on update (Trading style)
                if (state.table.highlightFrames[row][col] > 0) {
                    ImU32 bgColor = (state.table.directions[row][col] > 0) 
                        ? IM_COL32(0, 100, 0, 180)  // Dark green
                        : IM_COL32(100, 0, 0, 180); // Dark red
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, bgColor);
                    state.table.highlightFrames[row][col]--;
                }

                // Color code for text changes (simulate)
                ImVec4 color = ImVec4(1, 1, 1, 1); // Default white
                if (col == 9) { // Change column
                    color = value > 0 ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0.3f, 0.3f, 1);
                }
                
                ImGui::TextColored(color, "%.2f", value);
            }
        }
        
        ImGui::EndTable();
    }
    
    auto frameEnd = std::chrono::high_resolution_clock::now();
    auto frameTime = std::chrono::duration_cast<std::chrono::microseconds>(
        frameEnd - frameStart).count();
    
    if (state.testRunning) {
        state.table.recordFrame(frameTime);
    }
    
    ImGui::Separator();
    
    // Results display
    ImGui::TextWrapped("%s", state.resultsText);
    
    ImGui::Separator();
    ImGui::Text("Compare with Qt QTableView performance");
    ImGui::TextColored(ImVec4(1, 1, 0, 1), 
        "Run benchmark_model_update_methods for Qt comparison");
    
    ImGui::End();
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char** argv) {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    
    // GL 3.3 + GLSL 330
    const char* glsl_version = "#version 330";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    #ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif
    
    // Create window
    GLFWwindow* window = glfwCreateWindow(1400, 900, 
        "ImGui vs Qt Benchmark - Table Update Performance", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync
    
    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    // Setup style
    ImGui::StyleColorsDark();
    
    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    
    // Benchmark state
    BenchmarkState state;
    
    std::cout << "========================================\n";
    std::cout << "ImGui Table Update Benchmark\n";
    std::cout << "========================================\n";
    std::cout << "Configuration:\n";
    std::cout << "  Rows: " << NUM_ROWS << "\n";
    std::cout << "  Columns: " << NUM_COLS << "\n";
    std::cout << "  Test duration: " << TEST_DURATION_SEC << " seconds\n";
    std::cout << "========================================\n\n";
    
    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        // Update benchmark logic
        state.update();
        
        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Render UI
        RenderBenchmarkUI(state);
        
        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        glfwSwapBuffers(window);

        // Exit automatically when test finishes
        if (!state.testRunning && state.table.updateCount > 0) {
            break;
        }
    }
    
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}
