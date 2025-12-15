#!/bin/bash
# Script to update #include paths after refactoring

echo "Updating include paths in refactored codebase..."

# Function to update includes in a file
update_includes() {
    local file=$1
    echo "Processing: $file"
    
    # Core widgets
    sed -i '' 's|#include "ui/CustomMainWindow.h"|#include "core/widgets/CustomMainWindow.h"|g' "$file"
    sed -i '' 's|#include "ui/CustomMDIArea.h"|#include "core/widgets/CustomMDIArea.h"|g' "$file"
    sed -i '' 's|#include "ui/CustomMDISubWindow.h"|#include "core/widgets/CustomMDISubWindow.h"|g' "$file"
    sed -i '' 's|#include "ui/CustomMDIChild.h"|#include "core/widgets/CustomMDIChild.h"|g' "$file"
    sed -i '' 's|#include "ui/CustomTitleBar.h"|#include "core/widgets/CustomTitleBar.h"|g' "$file"
    sed -i '' 's|#include "ui/CustomScripComboBox.h"|#include "core/widgets/CustomScripComboBox.h"|g' "$file"
    sed -i '' 's|#include "ui/MDITaskBar.h"|#include "core/widgets/MDITaskBar.h"|g' "$file"
    sed -i '' 's|#include "ui/InfoBar.h"|#include "core/widgets/InfoBar.h"|g' "$file"
    
    # App layer
    sed -i '' 's|#include "ui/MainWindow.h"|#include "app/MainWindow.h"|g' "$file"
    sed -i '' 's|#include "ui/ScripBar.h"|#include "app/ScripBar.h"|g' "$file"
    
    # Views
    sed -i '' 's|#include "ui/BuyWindow.h"|#include "views/BuyWindow.h"|g' "$file"
    sed -i '' 's|#include "ui/SellWindow.h"|#include "views/SellWindow.h"|g' "$file"
    sed -i '' 's|#include "ui/MarketWatchWindow.h"|#include "views/MarketWatchWindow.h"|g' "$file"
    sed -i '' 's|#include "ui/SnapQuoteWindow.h"|#include "views/SnapQuoteWindow.h"|g' "$file"
    sed -i '' 's|#include "ui/PositionWindow.h"|#include "views/PositionWindow.h"|g' "$file"
    
    # Models
    sed -i '' 's|#include "ui/MarketWatchModel.h"|#include "models/MarketWatchModel.h"|g' "$file"
    sed -i '' 's|#include "ui/TokenAddressBook.h"|#include "models/TokenAddressBook.h"|g' "$file"
    
    # Services
    sed -i '' 's|#include "api/TokenSubscriptionManager.h"|#include "services/TokenSubscriptionManager.h"|g' "$file"
    
    # Repository
    sed -i '' 's|#include "data/Greeks.h"|#include "repository/Greeks.h"|g' "$file"
    sed -i '' 's|#include "data/ScripMaster.h"|#include "repository/ScripMaster.h"|g' "$file"
}

# Update all .cpp and .h files in new structure
find src/core -type f \( -name "*.cpp" -o -name "*.h" \) -exec bash -c 'update_includes "$0"' {} \;
find src/app -type f \( -name "*.cpp" -o -name "*.h" \) -exec bash -c 'update_includes "$0"' {} \;
find src/views -type f \( -name "*.cpp" -o -name "*.h" \) -exec bash -c 'update_includes "$0"' {} \;
find src/models -type f \( -name "*.cpp" -o -name "*.h" \) -exec bash -c 'update_includes "$0"' {} \;
find src/services -type f \( -name "*.cpp" -o -name "*.h" \) -exec bash -c 'update_includes "$0"' {} \;
find src/repository -type f \( -name "*.cpp" -o -name "*.h" \) -exec bash -c 'update_includes "$0"' {} \;

find include/core -type f \( -name "*.cpp" -o -name "*.h" \) -exec bash -c 'update_includes "$0"' {} \;
find include/app -type f \( -name "*.cpp" -o -name "*.h" \) -exec bash -c 'update_includes "$0"' {} \;
find include/views -type f \( -name "*.cpp" -o -name "*.h" \) -exec bash -c 'update_includes "$0"' {} \;
find include/models -type f \( -name "*.cpp" -o -name "*.h" \) -exec bash -c 'update_includes "$0"' {} \;
find include/services -type f \( -name "*.cpp" -o -name "*.h" \) -exec bash -c 'update_includes "$0"' {} \;
find include/repository -type f \( -name "*.cpp" -o -name "*.h" \) -exec bash -c 'update_includes "$0"' {} \;

# Export function
export -f update_includes

# Run updates
echo "Updating core/widgets..."
find src/core/widgets include/core/widgets -type f \( -name "*.cpp" -o -name "*.h" \) | while read file; do
    update_includes "$file"
done

echo "Updating app..."
find src/app include/app -type f \( -name "*.cpp" -o -name "*.h" \) | while read file; do
    update_includes "$file"
done

echo "Updating views..."
find src/views include/views -type f \( -name "*.cpp" -o -name "*.h" \) | while read file; do
    update_includes "$file"
done

echo "Updating models..."
find src/models include/models -type f \( -name "*.cpp" -o -name "*.h" \) | while read file; do
    update_includes "$file"
done

echo "Updating services..."
find src/services include/services -type f \( -name "*.cpp" -o -name "*.h" \) | while read file; do
    update_includes "$file"
done

echo "Updating repository..."
find src/repository include/repository -type f \( -name "*.cpp" -o -name "*.h" \) | while read file; do
    update_includes "$file"
done

echo "Include path updates complete!"
