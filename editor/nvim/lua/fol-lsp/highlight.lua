local M = {}
local files_mod = require('fol-lsp.files')

--- Scan a file for @def(name) declarations.
--- Returns a set of defined predicate names.
local function scan_file_defs(filepath)
  local defs = {}
  local f = io.open(filepath, 'r')
  if not f then return defs end

  for line in f:lines() do
    local name = line:match('@def%(([%w_]+)%)')
    if name then
      defs[name] = true
    end
  end
  f:close()
  return defs
end

--- Collect all @def predicate names from dependency files (everything except the buffer file).
local function collect_dep_predicates(bufpath)
  local all_files = files_mod.collect_files(bufpath)
  local bufpath_abs = vim.fn.fnamemodify(bufpath, ':p')
  local all_defs = {}

  for _, filepath in ipairs(all_files) do
    if filepath ~= bufpath_abs then
      local defs = scan_file_defs(filepath)
      for name in pairs(defs) do
        all_defs[name] = true
      end
    end
  end

  return all_defs
end

--- Scan the current buffer for @def lines and return {name = line_number}.
local function scan_buffer_defs(bufnr)
  local defs = {}
  local lines = vim.api.nvim_buf_get_lines(bufnr, 0, -1, false)
  for i, line in ipairs(lines) do
    local name = line:match('@def%(([%w_]+)%)')
    if name then
      defs[name] = i
    end
  end
  return defs
end

local ns = vim.api.nvim_create_namespace('fol_defined_predicates')

--- Highlight all defined predicates in the buffer.
function M.update(bufnr)
  bufnr = bufnr or vim.api.nvim_get_current_buf()
  vim.api.nvim_buf_clear_namespace(bufnr, ns, 0, -1)

  local filepath = vim.api.nvim_buf_get_name(bufnr)
  if filepath == '' then return end

  -- Predicates from dependencies: valid on all lines
  local dep_defs = collect_dep_predicates(filepath)

  -- Predicates defined in the current buffer: valid only after their @def line
  local local_defs = scan_buffer_defs(bufnr)

  local lines = vim.api.nvim_buf_get_lines(bufnr, 0, -1, false)
  for i, line in ipairs(lines) do
    local pos = 1
    while pos <= #line do
      local s, e, name = line:find('([a-zA-Z_][a-zA-Z0-9_]*)%s*%(', pos)
      if not s then break end

      local defined = dep_defs[name]
        or (local_defs[name] and i > local_defs[name])

      if defined then
        vim.api.nvim_buf_set_extmark(bufnr, ns, i - 1, s - 1, {
          end_col = s - 1 + #name,
          hl_group = 'FolDefinedPredicate',
        })
      end
      pos = e + 1
    end
  end
end

return M
