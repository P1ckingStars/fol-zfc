local M = {}

--- Resolve an include path relative to a directory.
function M.resolve_include(source_dir, inc_path)
  if inc_path:sub(1, 1) == '/' then
    return inc_path
  end
  return vim.fn.simplify(source_dir .. '/' .. inc_path)
end

--- For a .fol.proof file, return the corresponding .fol.def header.
--- For any other file, return the file itself.
function M.find_header(filepath)
  local header = filepath:gsub('%.fol%.proof$', '.fol.def')
  if header ~= filepath and vim.fn.filereadable(header) == 1 then
    return header
  end
  return filepath
end

--- Collect all files reachable through include directives (BFS).
--- For .fol.proof files, starts from the corresponding .fol.def header.
--- Returns a list of absolute file paths (header first).
function M.collect_files(start_file)
  start_file = vim.fn.fnamemodify(start_file, ':p')
  local header = M.find_header(start_file)
  header = vim.fn.fnamemodify(header, ':p')

  local seen = { [header] = true }
  local queue = { header }
  local result = { header }
  local head = 1

  while head <= #queue do
    local file = queue[head]
    head = head + 1
    local dir = vim.fn.fnamemodify(file, ':h')

    local f = io.open(file, 'r')
    if f then
      for line in f:lines() do
        local inc = line:match('^%s*include%s+"([^"]+)"')
        if inc then
          local resolved = M.resolve_include(dir, inc)
          resolved = vim.fn.fnamemodify(resolved, ':p')
          if not seen[resolved] then
            seen[resolved] = true
            queue[#queue + 1] = resolved
            result[#result + 1] = resolved
          end
        end
      end
      f:close()
    end
  end

  -- If we started from a .proof file, include it too (for label lookups)
  if header ~= vim.fn.fnamemodify(start_file, ':p') then
    table.insert(result, 1, vim.fn.fnamemodify(start_file, ':p'))
  end

  return result
end

return M
