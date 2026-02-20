local M = {}
local files = require('fol-lsp.files')

--- Search a file for a definition pattern. Returns (lnum, col) or nil.
local function find_definition_in_file(filepath, name)
  local f = io.open(filepath, 'r')
  if not f then return nil end

  local patterns = {
    '^%s*axiom%s+' .. vim.pesc(name) .. '%s*:',
    '^%s*claim%s+' .. vim.pesc(name) .. '%s*:',
    '^%s*theorem%s+' .. vim.pesc(name) .. '%s*:',
    '^%s*@def%(' .. vim.pesc(name) .. '%)',
  }

  local lnum = 0
  for line in f:lines() do
    lnum = lnum + 1
    for _, pat in ipairs(patterns) do
      local s = line:find(pat)
      if s then
        f:close()
        local col = line:find(vim.pesc(name), s)
        return lnum, (col or s) - 1
      end
    end
  end
  f:close()
  return nil
end

--- Search for a step label definition within the current proof block.
local function find_label_in_proof(bufnr, cursor_line, name)
  local proof_start = nil
  for i = cursor_line, 1, -1 do
    local line = vim.api.nvim_buf_get_lines(bufnr, i - 1, i, false)[1]
    if line:match('^%s*proof%s+%w+%s*:') then
      proof_start = i
      break
    end
  end
  if not proof_start then return nil end

  local total = vim.api.nvim_buf_line_count(bufnr)
  local pat = '^%s+' .. vim.pesc(name) .. '%s*='
  for i = proof_start, total do
    local line = vim.api.nvim_buf_get_lines(bufnr, i - 1, i, false)[1]
    if i > proof_start and line:match('^%s*proof%s+%w+%s*:') then
      break
    end
    if line:match(pat) then
      local col = line:find(vim.pesc(name))
      return i, (col or 1) - 1
    end
  end
  return nil
end

--- Jump to the definition of the word under the cursor.
function M.goto_definition()
  local bufnr = vim.api.nvim_get_current_buf()
  local cursor = vim.api.nvim_win_get_cursor(0)
  local line_nr = cursor[1]
  local line = vim.api.nvim_buf_get_lines(bufnr, line_nr - 1, line_nr, false)[1]

  -- Case 1: cursor on an include line -> open the file
  local inc_path = line:match('^%s*include%s+"([^"]+)"')
  if inc_path then
    local dir = vim.fn.fnamemodify(vim.api.nvim_buf_get_name(bufnr), ':h')
    local resolved = files.resolve_include(dir, inc_path)
    if vim.fn.filereadable(resolved) == 1 then
      vim.cmd('edit ' .. vim.fn.fnameescape(resolved))
    else
      vim.notify('File not found: ' .. resolved, vim.log.levels.WARN)
    end
    return
  end

  local word = vim.fn.expand('<cword>')
  if word == '' then return end

  -- Case 2: step label reference within current proof block
  local label_line, label_col = find_label_in_proof(bufnr, line_nr, word)
  if label_line and label_line ~= line_nr then
    vim.api.nvim_win_set_cursor(0, { label_line, label_col })
    return
  end

  -- Case 3: axiom/claim/predicate definition across file graph
  local current_file = vim.api.nvim_buf_get_name(bufnr)
  local all_files = files.collect_files(current_file)

  for _, filepath in ipairs(all_files) do
    local lnum, col = find_definition_in_file(filepath, word)
    if lnum then
      if vim.fn.fnamemodify(filepath, ':p') ~= vim.fn.fnamemodify(current_file, ':p') then
        vim.cmd('edit ' .. vim.fn.fnameescape(filepath))
      end
      vim.api.nvim_win_set_cursor(0, { lnum, col })
      return
    end
  end

  vim.notify('No definition found for: ' .. word, vim.log.levels.INFO)
end

return M
