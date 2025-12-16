# Development Roadmap - Trading Terminal C++

**Last Updated:** December 12, 2025  
**Current Status:** Stability Phase - Core Architecture Fixed  
**Next Milestone:** Feature Validation & Testing

---

## 🎯 Project Vision

Build a **professional, cross-platform trading terminal** with:
- Native performance (C++ & Qt)
- Modern UI/UX (VS Code-inspired)
- Frameless window with custom controls
- Multi-window MDI interface
- Real-time market data integration
- Order management system

---

## ✅ Completed Work

### **December 12, 2025 - Critical Stability Fix**

**Problem:** Application crashed on startup (segmentation fault)

**Solution:** Complete architectural redesign
- Removed conflicting `setMenuWidget()` usage
- Redesigned window hierarchy using manual layout
- Fixed title bar positioning (now at top)
- Eliminated `QMainWindow::menuBar()` conflicts

**Impact:**
- ✅ Application now runs stably
- ✅ Correct visual hierarchy restored
- ✅ Foundation for future development solid

**Files Modified:**
- `src/ui/CustomMainWindow.cpp` - Core window architecture
- `src/ui/MainWindow.cpp` - Application-specific layout

**Documentation Created:**
- `docs/CRITICAL_FIX_SUMMARY.md` - Detailed fix analysis
- `docs/TESTING_GUIDE.md` - Comprehensive test plan

---

## 🚀 Current Sprint: Validation & Testing

### **Goals:**
1. Validate all existing features work correctly
2. Identify any regressions from architecture changes
3. Document current state comprehensively
4. Establish baseline for future development

### **Tasks:**

#### ✅ Completed
- [x] Fix critical crash
- [x] Restore correct layout hierarchy
- [x] Create testing documentation
- [x] Verify basic window operations

#### 🔄 In Progress
- [ ] **Phase 2: Window Management Testing**
  - Test window dragging
  - Test 8-direction resizing
  - Test minimize/maximize/close
  - Verify geometry persistence
  
- [ ] **Phase 3: Menu Functionality Testing**
  - Verify all menu items work
  - Test keyboard shortcuts
  - Check menu item states (checkable items)

#### 📋 Backlog
- [ ] Phase 4: Toolbar functionality
- [ ] Phase 5: MDI child windows
- [ ] Phase 6: Status bar
- [ ] Phase 7: Visual theme consistency
- [ ] Phase 8: State persistence
- [ ] Phase 9: Stress testing

---

## 📅 Sprint 2: Feature Restoration (Est. 1-2 weeks)

### **Goals:**
- Restore full functionality to all features
- Fix any issues discovered during testing
- Improve UX where needed

### **Tasks:**

#### High Priority
- [ ] **Info Bar Restoration**
  - Currently uses QDockWidget (incompatible with manual layout)
  - Options:
    1. Convert to manual widget in layout
    2. Create custom docking system
    3. Make it a floating window
  - Decision needed on approach

- [ ] **Toolbar Enhancements**
  - Add drag handles if movability needed
  - Implement custom toolbar docking (optional)
  - Add visual separators
  - Fix any styling inconsistencies

- [ ] **Child Window System**
  - Verify Market Watch creation
  - Verify Buy Window creation
  - Test window arrangement (cascade, tile)
  - Ensure all child window features work

#### Medium Priority
- [ ] **State Persistence**
  - Implement custom save/restore for window geometry
  - Save/restore toolbar positions (if movable)
  - Save/restore child window states
  - Use QSettings for all preferences

- [ ] **Keyboard Shortcuts**
  - Verify Ctrl+M (New Market Watch)
  - Verify Ctrl+B (New Buy Window)
  - Add more shortcuts as needed
  - Document all shortcuts

- [ ] **Scrip Bar Integration**
  - Test search functionality
  - Verify cascade dropdowns work
  - Test "Add to Watch" signal
  - Ensure data flows correctly

#### Low Priority
- [ ] **Visual Polish**
  - Adjust toolbar heights for consistency
  - Fine-tune spacing and padding
  - Add icons to menu items
  - Improve hover effects
  - Add tooltips

---

## 📅 Sprint 3: Cross-Platform Validation (Est. 1 week)

### **Goals:**
- Ensure application works on all target platforms
- Fix platform-specific issues
- Optimize for each OS

### **Tasks:**

#### Linux Testing
- [ ] **Build on Ubuntu 22.04**
  - Test with X11 window manager
  - Test with Wayland
  - Verify all Qt dependencies available
  - Document build process

- [ ] **Build on Fedora 38+**
  - Test with GNOME
  - Test with KDE
  - Document any Fedora-specific requirements

- [ ] **Functionality Testing**
  - Frameless window behavior
  - Window resizing
  - Title bar controls
  - All features from testing guide

#### Windows Testing
- [ ] **Build with MinGW**
  - Set up build environment
  - Resolve any Windows-specific build issues
  - Test on Windows 10
  - Test on Windows 11

- [ ] **Build with MSVC**
  - Configure Visual Studio project
  - Build and test
  - Compare performance with MinGW

- [ ] **Windows-Specific Features**
  - Aero Snap integration
  - Taskbar integration
  - High DPI support
  - Multiple monitor support

#### macOS Validation
- [ ] **Intel Mac Testing**
  - Build for x86_64 architecture
  - Test on Intel Macs
  - Verify Rosetta compatibility not needed

- [ ] **Universal Binary**
  - Create fat binary (Intel + Apple Silicon)
  - Test on both architectures
  - Optimize for each architecture

---

## 📅 Sprint 4: Market Data Integration (Est. 2 weeks)

### **Goals:**
- Integrate XTS API for market data
- Display real-time quotes
- Handle WebSocket connections

### **Tasks:**

#### API Integration
- [ ] **XTS API Client**
  - Implement authentication
  - Implement market data subscription
  - Handle WebSocket messages
  - Parse and store data

- [ ] **Data Flow Architecture**
  - Design data distribution system
  - Implement observer pattern for updates
  - Handle throttling and rate limits
  - Error handling and reconnection

#### Market Watch Implementation
- [ ] **Data Display**
  - Show real-time prices
  - Update UI on data changes
  - Format numbers correctly
  - Color code price changes

- [ ] **Performance Optimization**
  - Minimize UI updates
  - Batch updates where possible
  - Use efficient data structures
  - Profile and optimize

---

## 📅 Sprint 5: Order Management (Est. 2-3 weeks)

### **Goals:**
- Implement order entry
- Display order book
- Handle order lifecycle

### **Tasks:**

#### Buy/Sell Windows
- [ ] **Order Entry Form**
  - Symbol selection
  - Quantity input
  - Price input (limit/market)
  - Order type selection
  - Validation

- [ ] **Order Submission**
  - API integration
  - Order confirmation
  - Error handling
  - Success feedback

#### Order Book
- [ ] **Display Orders**
  - List all orders
  - Show order status
  - Real-time updates
  - Filtering and sorting

- [ ] **Order Actions**
  - Modify order
  - Cancel order
  - Bulk operations

---

## 📅 Future Enhancements

### **Advanced Features**
- [ ] Charts and technical analysis
- [ ] Portfolio management
- [ ] Risk management tools
- [ ] Strategy backtesting
- [ ] Alert system
- [ ] News feed integration

### **UX Improvements**
- [ ] Customizable layouts
- [ ] Theme customization
- [ ] Workspace management
- [ ] Keyboard shortcut customization
- [ ] Multi-monitor support optimization

### **Performance**
- [ ] Memory usage optimization
- [ ] CPU usage optimization
- [ ] Startup time improvement
- [ ] Data caching
- [ ] Lazy loading

### **Infrastructure**
- [ ] Logging system
- [ ] Crash reporting
- [ ] Analytics
- [ ] Auto-updates
- [ ] Installer creation

---

## 🎓 Learning & Documentation

### **Developer Guides**
- [x] Critical Fix Summary
- [x] Testing Guide
- [ ] Architecture Guide
- [ ] API Integration Guide
- [ ] Building Guide (all platforms)
- [ ] Contributing Guide

### **User Documentation**
- [ ] User Manual
- [ ] Quick Start Guide
- [ ] Keyboard Shortcuts Reference
- [ ] Troubleshooting Guide
- [ ] FAQ

---

## 📊 Metrics & Goals

### **Code Quality**
- **Current:** Basic functionality, recently stabilized
- **Target:** 
  - 80%+ test coverage
  - Zero critical bugs
  - <5 medium bugs
  - Clean architecture

### **Performance**
- **Current:** Not measured
- **Target:**
  - Startup: <2 seconds
  - UI updates: 60 FPS
  - Memory: <500MB typical usage
  - CPU: <5% idle, <30% during heavy updates

### **Platform Support**
- **Current:** macOS Apple Silicon (working)
- **Target:**
  - macOS (Intel & Apple Silicon) ✅
  - Linux (Ubuntu, Fedora) 🔄
  - Windows (10, 11) 🔄

---

## 🤝 Team & Resources

### **Current Team**
- Developer: Active development
- QA: Manual testing in progress

### **Needed Resources**
- [ ] Windows testing machine
- [ ] Linux testing machine  
- [ ] CI/CD pipeline setup
- [ ] Code review process

---

## 📝 Decision Log

### **Architecture Decisions**

**Decision #1: Manual Layout vs QMainWindow Native APIs**
- **Date:** December 12, 2025
- **Context:** Crash due to `setMenuWidget()` conflict
- **Decision:** Use manual QVBoxLayout for all widgets
- **Rationale:** 
  - Eliminates conflicts
  - Full control over layout
  - Predictable behavior
  - Trade-off: Lost native toolbar docking
- **Status:** Implemented

**Decision #2: Custom MenuBar Widget**
- **Date:** December 12, 2025
- **Context:** Cannot use `QMainWindow::menuBar()` with custom title bar
- **Decision:** Create custom QMenuBar widget
- **Rationale:**
  - Works with manual layout
  - No conflicts with title bar
  - Full styling control
- **Status:** Implemented

---

## 🎯 Success Criteria

### **Sprint 1 (Current) - COMPLETE**
- [x] Application runs without crashing
- [x] Title bar at correct position
- [x] Basic window operations work
- [x] Testing documentation created

### **Sprint 2 - In Progress**
- [ ] All existing features functional
- [ ] No regressions from fixes
- [ ] All tests passing
- [ ] User-ready for alpha testing

### **Sprint 3**
- [ ] Works on all 3 platforms
- [ ] Platform-specific issues resolved
- [ ] Build instructions documented

### **Sprint 4**
- [ ] Real-time market data flowing
- [ ] Market Watch fully functional
- [ ] Stable WebSocket connections

### **Sprint 5**
- [ ] Can place orders successfully
- [ ] Order book shows live orders
- [ ] Basic trading workflow complete

---

## 📞 Support & Communication

### **Bug Reports**
- Use GitHub Issues
- Include platform, OS version, steps to reproduce
- Attach logs if applicable

### **Feature Requests**
- Use GitHub Discussions
- Describe use case and expected behavior
- Consider submitting PR

### **Development Questions**
- Check documentation first
- Use GitHub Discussions for Q&A
- Tag appropriately

---

## 🔄 Review & Update Schedule

This roadmap should be reviewed and updated:
- **Weekly:** During active development
- **Monthly:** During maintenance
- **After major milestones:** Document lessons learned

**Next Review:** After Sprint 1 completion (testing validation)

---

**Remember:** The goal is not speed, but building a solid, professional trading platform that works reliably across all platforms. Quality over quantity. Stability over features.
