#include "fzx/tui/line_editor.hpp"

#include "fzx/tui/key.hpp"

namespace fzx {

bool LineEditor::handle(uint8_t key)
{
  if (std::isprint(key)) {
    mLine.push_back(char(key));
    return true;
  } else if (key == kBackspace) {
    if (mLine.empty())
      return true;
    mLine.pop_back();
    return true;
  } else {
    return false;
  }
}

} // namespace fzx
