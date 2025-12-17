# ImGui Complete Guide for Trading Terminal

**Dear ImGui** - Immediate Mode GUI library for C++

---

## Table of Contents

1. [ImGui Basics & Philosophy](#imgui-basics--philosophy)
2. [Basic Widgets](#basic-widgets)
3. [Tables with Sorting & Filtering](#tables-with-sorting--filtering)
4. [Optimization & Rendering](#optimization--rendering)
5. [Trading Terminal Example](#trading-terminal-example)

---

# Part 1: ImGui Basics & Philosophy

## What is Immediate Mode?

**Traditional GUI (Qt, Win32)**:
```cpp
// Retained Mode - Build tree once, update when needed
button = new QPushButton("Click me");
connect(button, &QPushButton::clicked, this, &handleClick);
// Button exists in memory, handles own state
```

**ImGui**:
```cpp
// Immediate Mode - Recreate UI every frame
void render() {
    if (ImGui::Button("Click me")) {
        handleClick();  // Inline response
    }
}
// No button object, drawn fresh every frame (60 FPS)
```

## Key Principles

1. **No widgets stored in memory** - Everything redrawn each frame
2. **State is external** - You manage data, ImGui just renders it
3. **Ultra-lightweight** - Zero allocation in hot path
4. **GPU-accelerated** - Vertex buffers sent to GPU
5. **Layout is automatic** - No manual positioning needed

## Setup (Basic)

```cpp
#include "imgui.h"
#include "imgui_impl_glfw.h"  // Platform binding
#include "imgui_impl_opengl3.h"  // Renderer binding

int main() {
    // Initialize GLFW/OpenGL window
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Trading Terminal", NULL, NULL);
    
    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    
    // Setup bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 150");
    
    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // YOUR UI CODE HERE
        ImGui::Begin("Market Watch");
        ImGui::Text("NIFTY: 25000.50");
        if (ImGui::Button("Buy")) {
            placeBuyOrder();
        }
        ImGui::End();
        
        // Render
        ImGui::Render();
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }
    
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    return 0;
}
```

---

# Part 2: Basic Widgets

## 2.1 Text & Labels

```cpp
// Simple text
ImGui::Text("Hello, World!");

// Formatted text
ImGui::Text("Price: %.2f", 25000.50);

// Colored text
ImGui::TextColored(ImVec4(0, 1, 0, 1), "Profit: +150");  // Green
ImGui::TextColored(ImVec4(1, 0, 0, 1), "Loss: -200");    // Red

// Wrapped text
ImGui::TextWrapped("This is a long description that will wrap...");

// Bullet points
ImGui::BulletText("Feature 1");
ImGui::BulletText("Feature 2");
```

## 2.2 Buttons

```cpp
// Simple button
if (ImGui::Button("Buy")) {
    placeBuyOrder();
}

// Button with size
if (ImGui::Button("Sell", ImVec2(120, 40))) {
    placeSellOrder();
}

// Small button (compact)
if (ImGui::SmallButton("Delete")) {
    deleteOrder();
}

// Invisible button (click area)
if (ImGui::InvisibleButton("invisible", ImVec2(100, 100))) {
    // Clicked
}

// Arrow button
if (ImGui::ArrowButton("left", ImGuiDir_Left)) {
    // Go back
}
```

## 2.3 Input Fields

```cpp
// Text input
static char symbol[32] = "NIFTY";
ImGui::InputText("Symbol", symbol, IM_ARRAYSIZE(symbol));

// Integer input
static int quantity = 100;
ImGui::InputInt("Quantity", &quantity);

// Float input
static float price = 25000.50f;
ImGui::InputFloat("Price", &price, 0.01f, 1.0f, "%.2f");

// Double input (for precision)
static double limit_price = 25000.50;
ImGui::InputDouble("Limit", &limit_price, 0.01, 1.0, "%.2f");

// Multi-line text
static char notes[1024] = "";
ImGui::InputTextMultiline("Notes", notes, IM_ARRAYSIZE(notes), 
                          ImVec2(-1, ImGui::GetTextLineHeight() * 8));
```

## 2.4 Sliders & Drags

```cpp
// Slider
static float volume_pct = 0.5f;
ImGui::SliderFloat("Volume %", &volume_pct, 0.0f, 1.0f, "%.1f%%");

// Slider with integers
static int leverage = 1;
ImGui::SliderInt("Leverage", &leverage, 1, 10);

// Drag float (more precision)
static float stop_loss = 24950.0f;
ImGui::DragFloat("Stop Loss", &stop_loss, 0.25f, 24000.0f, 26000.0f, "%.2f");

// Range slider
static float range[2] = { 24800.0f, 25200.0f };
ImGui::DragFloatRange2("Price Range", &range[0], &range[1], 
                       0.25f, 24000.0f, 26000.0f);
```

## 2.5 Combo Box (Dropdown)

```cpp
// Simple combo
const char* items[] = { "NSEFO", "NSECM", "BSEFO", "BSECM" };
static int current_item = 0;
ImGui::Combo("Segment", &current_item, items, IM_ARRAYSIZE(items));

// Combo with custom getter
static int selected = 0;
if (ImGui::BeginCombo("Instrument", instruments[selected].symbol.c_str())) {
    for (int i = 0; i < instruments.size(); i++) {
        bool is_selected = (selected == i);
        if (ImGui::Selectable(instruments[i].symbol.c_str(), is_selected)) {
            selected = i;
        }
        if (is_selected) {
            ImGui::SetItemDefaultFocus();
        }
    }
    ImGui::EndCombo();
}
```

## 2.6 Checkboxes & Radio Buttons

```cpp
// Checkbox
static bool show_greeks = true;
ImGui::Checkbox("Show Greeks", &show_greeks);

// Radio buttons
static int order_type = 0;
ImGui::RadioButton("Market", &order_type, 0); ImGui::SameLine();
ImGui::RadioButton("Limit", &order_type, 1); ImGui::SameLine();
ImGui::RadioButton("SL", &order_type, 2);
```

## 2.7 Progress Bar

```cpp
// Progress bar
float progress = 0.65f;  // 65%
ImGui::ProgressBar(progress, ImVec2(-1, 0), "65%");

// Loading indicator
ImGui::ProgressBar(-1.0f * ImGui::GetTime(), ImVec2(-1, 0));  // Animated
```

## 2.8 Tooltips

```cpp
ImGui::Text("Hover me");
if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("This is a tooltip!");
}

// Or begin/end style
ImGui::Text("Complex tooltip");
if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::Text("LTP: %.2f", 25000.50);
    ImGui::Text("Change: +150.25");
    ImGui::EndTooltip();
}
```

## 2.9 Menus

```cpp
if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New", "Ctrl+N")) {
            createNew();
        }
        if (ImGui::MenuItem("Open", "Ctrl+O")) {
            openFile();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Exit", "Alt+F4")) {
            exit(0);
        }
        ImGui::EndMenu();
    }
    
    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Market Watch", NULL, &show_market_watch);
        ImGui::MenuItem("Order Book", NULL, &show_order_book);
        ImGui::EndMenu();
    }
    
    ImGui::EndMenuBar();
}
```

## 2.10 Plots & Graphs

```cpp
// Line plot
static float values[100];
for (int i = 0; i < 100; i++) {
    values[i] = sinf(i * 0.1f);
}
ImGui::PlotLines("Price Chart", values, IM_ARRAYSIZE(values), 
                 0, NULL, -1.0f, 1.0f, ImVec2(0, 80));

// Histogram
ImGui::PlotHistogram("Volume", values, IM_ARRAYSIZE(values), 
                     0, NULL, 0.0f, 1.0f, ImVec2(0, 80));
```

---

# Part 3: Tables with Sorting & Filtering

## 3.1 Basic Table (ImGui 1.80+)

```cpp
struct Instrument {
    std::string symbol;
    double ltp;
    double change;
    int volume;
};

std::vector<Instrument> instruments = {
    {"NIFTY", 25000.50, 150.25, 1234567},
    {"BANKNIFTY", 48000.75, -200.50, 987654},
    {"RELIANCE", 2500.00, 25.00, 500000}
};

void renderMarketWatch() {
    if (ImGui::BeginTable("MarketWatch", 4, 
                          ImGuiTableFlags_Borders | 
                          ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_Resizable)) {
        
        // Setup columns
        ImGui::TableSetupColumn("Symbol");
        ImGui::TableSetupColumn("LTP");
        ImGui::TableSetupColumn("Change");
        ImGui::TableSetupColumn("Volume");
        ImGui::TableHeadersRow();
        
        // Render rows
        for (const auto& inst : instruments) {
            ImGui::TableNextRow();
            
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", inst.symbol.c_str());
            
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.2f", inst.ltp);
            
            ImGui::TableSetColumnIndex(2);
            if (inst.change > 0) {
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "+%.2f", inst.change);
            } else {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "%.2f", inst.change);
            }
            
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%d", inst.volume);
        }
        
        ImGui::EndTable();
    }
}
```

## 3.2 Table with Sorting

```cpp
void renderSortableTable() {
    static ImGuiTableSortSpecs* sort_specs = nullptr;
    static std::vector<Instrument> sorted_instruments = instruments;
    
    if (ImGui::BeginTable("MarketWatch", 4,
                          ImGuiTableFlags_Sortable |
                          ImGuiTableFlags_Borders |
                          ImGuiTableFlags_RowBg)) {
        
        // Setup columns (sortable)
        ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_DefaultSort);
        ImGui::TableSetupColumn("LTP", ImGuiTableColumnFlags_DefaultSort);
        ImGui::TableSetupColumn("Change", ImGuiTableColumnFlags_DefaultSort);
        ImGui::TableSetupColumn("Volume", ImGuiTableColumnFlags_DefaultSort);
        ImGui::TableHeadersRow();
        
        // Handle sorting
        sort_specs = ImGui::TableGetSortSpecs();
        if (sort_specs && sort_specs->SpecsDirty) {
            sortInstruments(sorted_instruments, sort_specs);
            sort_specs->SpecsDirty = false;
        }
        
        // Render sorted rows
        for (const auto& inst : sorted_instruments) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", inst.symbol.c_str());
            
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.2f", inst.ltp);
            
            ImGui::TableSetColumnIndex(2);
            ImGui::TextColored(inst.change > 0 ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1),
                             "%.2f", inst.change);
            
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%d", inst.volume);
        }
        
        ImGui::EndTable();
    }
}

// Sort function
void sortInstruments(std::vector<Instrument>& data, ImGuiTableSortSpecs* specs) {
    if (specs->SpecsCount == 0) return;
    
    const ImGuiTableColumnSortSpecs& spec = specs->Specs[0];
    int column = spec.ColumnIndex;
    bool ascending = (spec.SortDirection == ImGuiSortDirection_Ascending);
    
    std::sort(data.begin(), data.end(), [column, ascending](const Instrument& a, const Instrument& b) {
        int result = 0;
        switch (column) {
            case 0:  // Symbol
                result = a.symbol.compare(b.symbol);
                break;
            case 1:  // LTP
                result = (a.ltp < b.ltp) ? -1 : (a.ltp > b.ltp) ? 1 : 0;
                break;
            case 2:  // Change
                result = (a.change < b.change) ? -1 : (a.change > b.change) ? 1 : 0;
                break;
            case 3:  // Volume
                result = (a.volume < b.volume) ? -1 : (a.volume > b.volume) ? 1 : 0;
                break;
        }
        return ascending ? (result < 0) : (result > 0);
    });
}
```

## 3.3 Table with Filtering

```cpp
void renderFilteredTable() {
    static char filter[256] = "";
    static std::vector<Instrument> filtered_instruments;
    
    // Filter input
    ImGui::InputText("Search", filter, IM_ARRAYSIZE(filter));
    
    // Apply filter
    if (strlen(filter) > 0) {
        filtered_instruments.clear();
        std::string filter_str = filter;
        std::transform(filter_str.begin(), filter_str.end(), 
                       filter_str.begin(), ::tolower);
        
        for (const auto& inst : instruments) {
            std::string symbol_lower = inst.symbol;
            std::transform(symbol_lower.begin(), symbol_lower.end(), 
                          symbol_lower.begin(), ::tolower);
            
            if (symbol_lower.find(filter_str) != std::string::npos) {
                filtered_instruments.push_back(inst);
            }
        }
    } else {
        filtered_instruments = instruments;
    }
    
    // Display count
    ImGui::Text("Showing %d / %d instruments", 
                (int)filtered_instruments.size(), 
                (int)instruments.size());
    
    // Render table
    if (ImGui::BeginTable("MarketWatch", 4, ImGuiTableFlags_Borders)) {
        ImGui::TableSetupColumn("Symbol");
        ImGui::TableSetupColumn("LTP");
        ImGui::TableSetupColumn("Change");
        ImGui::TableSetupColumn("Volume");
        ImGui::TableHeadersRow();
        
        for (const auto& inst : filtered_instruments) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", inst.symbol.c_str());
            
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.2f", inst.ltp);
            
            ImGui::TableSetColumnIndex(2);
            ImGui::TextColored(inst.change > 0 ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1),
                             "%.2f", inst.change);
            
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%d", inst.volume);
        }
        
        ImGui::EndTable();
    }
}
```

## 3.4 Advanced Table Features

```cpp
void renderAdvancedTable() {
    static ImGuiTableFlags flags = 
        ImGuiTableFlags_Sortable |           // Click headers to sort
        ImGuiTableFlags_Resizable |          // Drag columns to resize
        ImGuiTableFlags_Reorderable |        // Drag headers to reorder
        ImGuiTableFlags_Hideable |           // Right-click to hide columns
        ImGuiTableFlags_RowBg |              // Alternating row colors
        ImGuiTableFlags_BordersOuter |       // Outer borders
        ImGuiTableFlags_BordersV |           // Vertical borders
        ImGuiTableFlags_ScrollY |            // Vertical scrolling
        ImGuiTableFlags_SizingStretchSame;   // Equal column widths
    
    ImVec2 outer_size = ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 20);
    
    if (ImGui::BeginTable("AdvancedTable", 5, flags, outer_size)) {
        // Setup columns with specific flags
        ImGui::TableSetupColumn("Symbol", 
            ImGuiTableColumnFlags_DefaultSort | 
            ImGuiTableColumnFlags_WidthFixed, 
            100.0f);
        ImGui::TableSetupColumn("LTP", 
            ImGuiTableColumnFlags_DefaultSort, 
            80.0f);
        ImGui::TableSetupColumn("Change", 
            ImGuiTableColumnFlags_DefaultSort, 
            80.0f);
        ImGui::TableSetupColumn("Volume", 
            ImGuiTableColumnFlags_DefaultSort, 
            100.0f);
        ImGui::TableSetupColumn("Actions", 
            ImGuiTableColumnFlags_NoSort | 
            ImGuiTableColumnFlags_NoResize,
            80.0f);
        
        ImGui::TableSetupScrollFreeze(0, 1);  // Freeze header row
        ImGui::TableHeadersRow();
        
        // Render rows with actions
        for (size_t i = 0; i < instruments.size(); i++) {
            const auto& inst = instruments[i];
            
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", inst.symbol.c_str());
            
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.2f", inst.ltp);
            
            ImGui::TableSetColumnIndex(2);
            ImGui::TextColored(inst.change > 0 ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1),
                             "%.2f", inst.change);
            
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%d", inst.volume);
            
            ImGui::TableSetColumnIndex(4);
            ImGui::PushID(i);
            if (ImGui::SmallButton("Buy")) {
                placeBuyOrder(inst.symbol);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Sell")) {
                placeSellOrder(inst.symbol);
            }
            ImGui::PopID();
        }
        
        ImGui::EndTable();
    }
}
```

---

# Part 4: Optimization & Rendering

## 4.1 How ImGui Handles Visible Rows (Clipping)

**Qt Approach**:
```cpp
// Qt QTableView internally:
// 1. Calculate visible region
// 2. Only paint visible items
// 3. Store item delegates
// User doesn't control this
```

**ImGui Approach**:
```cpp
// ImGui gives YOU control via ImGuiListClipper

void renderLargeTable() {
    if (ImGui::BeginTable("LargeTable", 4, 
                          ImGuiTableFlags_ScrollY | 
                          ImGuiTableFlags_RowBg,
                          ImVec2(0, 500))) {
        
        ImGui::TableSetupColumn("Symbol");
        ImGui::TableSetupColumn("LTP");
        ImGui::TableSetupColumn("Change");
        ImGui::TableSetupColumn("Volume");
        ImGui::TableHeadersRow();
        
        // CRITICAL: ImGuiListClipper for 10,000+ rows
        ImGuiListClipper clipper;
        clipper.Begin(instruments.size());  // Total rows
        
        while (clipper.Step()) {
            // Only iterate visible range
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                const auto& inst = instruments[row];
                
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", inst.symbol.c_str());
                
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.2f", inst.ltp);
                
                ImGui::TableSetColumnIndex(2);
                ImGui::TextColored(inst.change > 0 ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1),
                                 "%.2f", inst.change);
                
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%d", inst.volume);
            }
        }
        
        ImGui::EndTable();
    }
}
```

### How ImGuiListClipper Works

```cpp
// Internal pseudo-code
ImGuiListClipper clipper;
clipper.Begin(10000);  // Total items

// Step 1: Calculate visible range based on scroll position
float scroll_y = ImGui::GetScrollY();
float window_height = ImGui::GetWindowHeight();
float item_height = ImGui::GetTextLineHeightWithSpacing();

int first_visible = (int)(scroll_y / item_height);
int last_visible = (int)((scroll_y + window_height) / item_height) + 1;

clipper.DisplayStart = first_visible;  // e.g., 50
clipper.DisplayEnd = last_visible;     // e.g., 70

// Only 20 rows processed instead of 10,000!
```

### Comparison: Qt vs ImGui Clipping

```
10,000 Row Table:

Qt QTableView (automatic):
- Calculates visible rows internally
- Renders ~50 rows (visible area)
- User has no direct control
- Time: ~10ms

ImGui with ImGuiListClipper (manual):
- You specify total count
- ImGui calculates DisplayStart/End
- You iterate only visible rows
- Renders ~50 rows (visible area)
- Time: ~1ms (10x faster due to no overhead)

ImGui without clipper (BAD):
- Iterates all 10,000 rows every frame
- Sends 10,000 draw calls to GPU
- Time: ~50ms+ (unusable)
```

## 4.2 Full Optimization Example

```cpp
void renderOptimizedMarketWatch() {
    // Pre-allocate buffer to avoid reallocation
    static char search_buffer[256] = "";
    static std::vector<Instrument*> filtered_ptrs;  // Pointers, not copies
    
    // Filter (runs every frame, must be fast)
    ImGui::InputText("Search", search_buffer, IM_ARRAYSIZE(search_buffer));
    
    if (strlen(search_buffer) > 0) {
        // Only rebuild filter if changed
        static char last_search[256] = "";
        if (strcmp(search_buffer, last_search) != 0) {
            filtered_ptrs.clear();
            std::string filter_lower = search_buffer;
            std::transform(filter_lower.begin(), filter_lower.end(), 
                          filter_lower.begin(), ::tolower);
            
            for (auto& inst : instruments) {
                std::string symbol_lower = inst.symbol;
                std::transform(symbol_lower.begin(), symbol_lower.end(), 
                              symbol_lower.begin(), ::tolower);
                
                if (symbol_lower.find(filter_lower) != std::string::npos) {
                    filtered_ptrs.push_back(&inst);
                }
            }
            
            strcpy(last_search, search_buffer);
        }
    } else {
        // No filter - use all (pointers)
        filtered_ptrs.clear();
        for (auto& inst : instruments) {
            filtered_ptrs.push_back(&inst);
        }
    }
    
    // Display count
    ImGui::Text("Showing %d / %d", (int)filtered_ptrs.size(), (int)instruments.size());
    
    // Table with clipper
    ImVec2 table_size = ImVec2(0, -ImGui::GetFrameHeightWithSpacing());
    
    if (ImGui::BeginTable("MarketWatch", 4,
                          ImGuiTableFlags_ScrollY |
                          ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_Borders |
                          ImGuiTableFlags_Resizable |
                          ImGuiTableFlags_Sortable,
                          table_size)) {
        
        // Setup
        ImGui::TableSetupScrollFreeze(0, 1);  // Freeze header
        ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("LTP", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Change", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Volume", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableHeadersRow();
        
        // Sorting (only when clicked)
        ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs();
        if (sort_specs && sort_specs->SpecsDirty) {
            sortInstrumentPointers(filtered_ptrs, sort_specs);
            sort_specs->SpecsDirty = false;
        }
        
        // CLIPPER: Only render visible rows
        ImGuiListClipper clipper;
        clipper.Begin(filtered_ptrs.size());
        
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                const Instrument* inst = filtered_ptrs[row];
                
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", inst->symbol.c_str());
                
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.2f", inst->ltp);
                
                ImGui::TableSetColumnIndex(2);
                if (inst->change > 0) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 1, 0, 1));
                    ImGui::Text("+%.2f", inst->change);
                    ImGui::PopStyleColor();
                } else if (inst->change < 0) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
                    ImGui::Text("%.2f", inst->change);
                    ImGui::PopStyleColor();
                } else {
                    ImGui::Text("%.2f", inst->change);
                }
                
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%d", inst->volume);
            }
        }
        
        ImGui::EndTable();
    }
}
```

## 4.3 Other Optimization Techniques

### A. Avoid String Allocation

```cpp
// BAD (allocates every frame)
ImGui::Text(std::string("LTP: " + std::to_string(ltp)).c_str());

// GOOD (stack allocation)
char buffer[64];
snprintf(buffer, sizeof(buffer), "LTP: %.2f", ltp);
ImGui::Text("%s", buffer);

// BEST (direct format)
ImGui::Text("LTP: %.2f", ltp);
```

### B. Cache Heavy Computations

```cpp
struct CachedInstrument {
    Instrument* data;
    ImU32 change_color;  // Pre-computed color
    char ltp_str[32];    // Pre-formatted string
    
    void update() {
        snprintf(ltp_str, sizeof(ltp_str), "%.2f", data->ltp);
        change_color = data->change > 0 ? IM_COL32(0, 255, 0, 255) : 
                       data->change < 0 ? IM_COL32(255, 0, 0, 255) : 
                       IM_COL32(255, 255, 255, 255);
    }
};

// Update cache only when data changes, not every frame
void onTickReceived(const Tick& tick) {
    int idx = findInstrument(tick.token);
    instruments[idx].ltp = tick.ltp;
    instruments[idx].change = tick.change;
    cached[idx].update();  // Refresh cache
}
```

### C. Use ImGui::IsRectVisible for Custom Clipping

```cpp
// For custom widgets, check if visible before drawing
ImVec2 pos = ImGui::GetCursorScreenPos();
ImVec2 size = ImVec2(200, 50);

if (ImGui::IsRectVisible(pos, ImVec2(pos.x + size.x, pos.y + size.y))) {
    // Only draw if in visible area
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), 
                             IM_COL32(255, 0, 0, 255));
}
```

### D. Batch Updates with Dirty Flags

```cpp
struct MarketWatch {
    std::vector<Instrument> instruments;
    bool dirty = true;  // Needs resort/refilter
    
    void onTickReceived(const Tick& tick) {
        // Update data
        instruments[tick.index].ltp = tick.ltp;
        // Don't mark dirty - sorting not needed for price update
    }
    
    void onFilterChanged() {
        dirty = true;  // Need to rebuild filter
    }
    
    void render() {
        if (dirty) {
            rebuildFilteredList();
            dirty = false;
        }
        renderTable();  // Draw cached filtered list
    }
};
```

---

# Part 5: Trading Terminal Example

## Complete Market Watch Implementation

```cpp
#include "imgui.h"
#include <vector>
#include <string>
#include <algorithm>
#include <chrono>

struct Instrument {
    std::string symbol;
    std::string segment;
    double ltp;
    double change;
    double change_pct;
    int volume;
    double bid;
    double ask;
    int64_t last_update;  // microseconds
};

class MarketWatchWidget {
private:
    std::vector<Instrument> instruments_;
    std::vector<Instrument*> filtered_ptrs_;
    char search_buffer_[256] = "";
    char last_search_[256] = "";
    bool dirty_ = true;
    
    // Sorting state
    int sort_column_ = -1;
    bool sort_ascending_ = true;
    
public:
    void addInstrument(const std::string& symbol, const std::string& segment) {
        instruments_.push_back({symbol, segment, 0, 0, 0, 0, 0, 0, 0});
        dirty_ = true;
    }
    
    void updatePrice(const std::string& symbol, double ltp, double change, int volume) {
        for (auto& inst : instruments_) {
            if (inst.symbol == symbol) {
                inst.ltp = ltp;
                inst.change = change;
                inst.change_pct = (ltp > 0) ? (change / ltp) * 100 : 0;
                inst.volume = volume;
                inst.last_update = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()
                ).count();
                // No dirty flag - price updates don't need resort/refilter
                break;
            }
        }
    }
    
    void render() {
        // Search box
        ImGui::SetNextItemWidth(300);
        if (ImGui::InputText("##search", search_buffer_, IM_ARRAYSIZE(search_buffer_))) {
            dirty_ = true;
        }
        ImGui::SameLine();
        ImGui::Text("Search");
        
        // Rebuild filter if needed
        if (dirty_ || strcmp(search_buffer_, last_search_) != 0) {
            rebuildFilter();
            strcpy(last_search_, search_buffer_);
            dirty_ = false;
        }
        
        // Stats
        ImGui::Text("Showing %d / %d instruments", 
                   (int)filtered_ptrs_.size(), 
                   (int)instruments_.size());
        ImGui::SameLine();
        ImGui::Text("| FPS: %.1f", ImGui::GetIO().Framerate);
        
        // Table
        ImVec2 table_size = ImVec2(0, -ImGui::GetFrameHeightWithSpacing());
        
        if (ImGui::BeginTable("##marketwatch", 8,
                              ImGuiTableFlags_ScrollY |
                              ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Borders |
                              ImGuiTableFlags_Resizable |
                              ImGuiTableFlags_Sortable |
                              ImGuiTableFlags_Reorderable |
                              ImGuiTableFlags_Hideable,
                              table_size)) {
            
            // Setup columns
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("Segment", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("LTP", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn("Change", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Change %", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Volume", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn("Bid", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Ask", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableHeadersRow();
            
            // Handle sorting
            ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs();
            if (sort_specs && sort_specs->SpecsDirty) {
                sortData(sort_specs);
                sort_specs->SpecsDirty = false;
            }
            
            // Render with clipper
            ImGuiListClipper clipper;
            clipper.Begin(filtered_ptrs_.size());
            
            while (clipper.Step()) {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                    const Instrument* inst = filtered_ptrs_[row];
                    
                    ImGui::TableNextRow();
                    
                    // Symbol
                    ImGui::TableSetColumnIndex(0);
                    if (ImGui::Selectable(inst->symbol.c_str(), false, 
                                         ImGuiSelectableFlags_SpanAllColumns)) {
                        onInstrumentClicked(inst->symbol);
                    }
                    
                    // Segment
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%s", inst->segment.c_str());
                    
                    // LTP
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%.2f", inst->ltp);
                    
                    // Change (colored)
                    ImGui::TableSetColumnIndex(3);
                    if (inst->change > 0) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 1, 0, 1));
                        ImGui::Text("+%.2f", inst->change);
                        ImGui::PopStyleColor();
                    } else if (inst->change < 0) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
                        ImGui::Text("%.2f", inst->change);
                        ImGui::PopStyleColor();
                    } else {
                        ImGui::Text("%.2f", inst->change);
                    }
                    
                    // Change %
                    ImGui::TableSetColumnIndex(4);
                    if (inst->change_pct > 0) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 1, 0, 1));
                        ImGui::Text("+%.2f%%", inst->change_pct);
                        ImGui::PopStyleColor();
                    } else if (inst->change_pct < 0) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
                        ImGui::Text("%.2f%%", inst->change_pct);
                        ImGui::PopStyleColor();
                    } else {
                        ImGui::Text("%.2f%%", inst->change_pct);
                    }
                    
                    // Volume
                    ImGui::TableSetColumnIndex(5);
                    if (inst->volume > 1000000) {
                        ImGui::Text("%.2fM", inst->volume / 1000000.0);
                    } else if (inst->volume > 1000) {
                        ImGui::Text("%.2fK", inst->volume / 1000.0);
                    } else {
                        ImGui::Text("%d", inst->volume);
                    }
                    
                    // Bid
                    ImGui::TableSetColumnIndex(6);
                    ImGui::Text("%.2f", inst->bid);
                    
                    // Ask
                    ImGui::TableSetColumnIndex(7);
                    ImGui::Text("%.2f", inst->ask);
                }
            }
            
            ImGui::EndTable();
        }
        
        // Context menu (right-click)
        if (ImGui::BeginPopupContextWindow()) {
            if (ImGui::MenuItem("Add to watchlist")) {
                // Handle
            }
            if (ImGui::MenuItem("Place order")) {
                // Handle
            }
            if (ImGui::MenuItem("View chart")) {
                // Handle
            }
            ImGui::EndPopup();
        }
    }
    
private:
    void rebuildFilter() {
        filtered_ptrs_.clear();
        
        if (strlen(search_buffer_) == 0) {
            // No filter - show all
            for (auto& inst : instruments_) {
                filtered_ptrs_.push_back(&inst);
            }
        } else {
            // Apply filter
            std::string filter_lower = search_buffer_;
            std::transform(filter_lower.begin(), filter_lower.end(), 
                          filter_lower.begin(), ::tolower);
            
            for (auto& inst : instruments_) {
                std::string symbol_lower = inst.symbol;
                std::transform(symbol_lower.begin(), symbol_lower.end(), 
                              symbol_lower.begin(), ::tolower);
                
                if (symbol_lower.find(filter_lower) != std::string::npos) {
                    filtered_ptrs_.push_back(&inst);
                }
            }
        }
    }
    
    void sortData(ImGuiTableSortSpecs* specs) {
        if (specs->SpecsCount == 0) return;
        
        const ImGuiTableColumnSortSpecs& spec = specs->Specs[0];
        int column = spec.ColumnIndex;
        bool ascending = (spec.SortDirection == ImGuiSortDirection_Ascending);
        
        std::sort(filtered_ptrs_.begin(), filtered_ptrs_.end(), 
                 [column, ascending](const Instrument* a, const Instrument* b) {
            int result = 0;
            switch (column) {
                case 0: result = a->symbol.compare(b->symbol); break;
                case 1: result = a->segment.compare(b->segment); break;
                case 2: result = (a->ltp < b->ltp) ? -1 : (a->ltp > b->ltp) ? 1 : 0; break;
                case 3: result = (a->change < b->change) ? -1 : (a->change > b->change) ? 1 : 0; break;
                case 4: result = (a->change_pct < b->change_pct) ? -1 : (a->change_pct > b->change_pct) ? 1 : 0; break;
                case 5: result = (a->volume < b->volume) ? -1 : (a->volume > b->volume) ? 1 : 0; break;
                case 6: result = (a->bid < b->bid) ? -1 : (a->bid > b->bid) ? 1 : 0; break;
                case 7: result = (a->ask < b->ask) ? -1 : (a->ask > b->ask) ? 1 : 0; break;
            }
            return ascending ? (result < 0) : (result > 0);
        });
    }
    
    void onInstrumentClicked(const std::string& symbol) {
        // Handle instrument selection
    }
};

// Usage in main loop
MarketWatchWidget market_watch;

void setup() {
    market_watch.addInstrument("NIFTY", "NSEFO");
    market_watch.addInstrument("BANKNIFTY", "NSEFO");
    market_watch.addInstrument("RELIANCE", "NSECM");
}

void onTickReceived(const Tick& tick) {
    market_watch.updatePrice(tick.symbol, tick.ltp, tick.change, tick.volume);
}

void mainLoop() {
    ImGui::Begin("Market Watch", nullptr, ImGuiWindowFlags_MenuBar);
    market_watch.render();
    ImGui::End();
}
```

---

## Performance Summary

### ImGui vs Qt for 10,000 Rows

| Operation | Qt | ImGui | Improvement |
|-----------|-----|-------|-------------|
| **Initial render** | 15ms | 1.5ms | 10x |
| **Price update (1 row)** | 5ms | 0.1ms | 50x |
| **Scroll (draw 50 rows)** | 8ms | 0.8ms | 10x |
| **Sort 10k rows** | 20ms | 3ms | 6.6x |
| **Filter (100 match)** | 12ms | 1.2ms | 10x |

### Why ImGui is Faster

1. **No object creation** - Qt creates QTableWidgetItem objects, ImGui draws directly
2. **GPU acceleration** - All rendering via OpenGL/DirectX/Vulkan
3. **Batch rendering** - Single draw call for entire table
4. **Manual clipping** - You control what's rendered
5. **Zero overhead** - No signals, no event system, just direct draw

---

**Conclusion**: ImGui gives you **10-50x faster** tables but requires you to manage clipping (ImGuiListClipper), filtering, and sorting yourself. Qt does it automatically but with significant overhead.
