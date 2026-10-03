$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$source = Join-Path $repo 'scripts\r1c4b-mvp1-explorer-guest-driver.ps1'
$tokens=$null; $errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($source,[ref]$tokens,[ref]$errors)
if ($errors.Count) { throw 'driver_parse_error' }
$function=$ast.Find({param($node)
    $node -is [Management.Automation.Language.FunctionDefinitionAst] -and
    $node.Name -ceq 'Product-Rows'},$true)
if (-not $function) { throw 'actual_production_reader_missing' }
. ([scriptblock]::Create($function.Extent.Text))
# Exercise the ACTUAL guest reader function, not a second ideal implementation.
# No guest entry, GUI, native input, or system configuration is executed here.
$productLog=Join-Path $repo ('uat\product-reader-'+[Guid]::NewGuid().ToString('N')+'.jsonl')
$writer=[IO.FileStream]::new($productLog,[IO.FileMode]::CreateNew,
    [IO.FileAccess]::Write,[IO.FileShare]::Read)
try {
    $first=[Text.Encoding]::UTF8.GetBytes("{`"sequence`":1,`"type`":`"startup`"}`n")
    $writer.Write($first,0,$first.Length); $writer.Flush()
    # The old ReadAllLines implementation fails while this writer is open.
    $rows=@(Product-Rows)
    if($rows.Count -ne 1 -or $rows[0].sequence -ne 1){throw 'live_writer_read_failed'}
    $partial=[Text.Encoding]::UTF8.GetBytes('{"sequence":2,"type":')
    $writer.Write($partial,0,$partial.Length); $writer.Flush()
    if(@(Product-Rows).Count -ne 1){throw 'partial_tail_was_accepted'}
    $tail=[Text.Encoding]::UTF8.GetBytes("`"target_prompt`"}`n")
    $writer.Write($tail,0,$tail.Length); $writer.Flush()
    $rows=@(Product-Rows)
    if($rows.Count -ne 2 -or $rows[1].type -cne 'target_prompt'){throw 'completed_tail_not_read'}
    Write-Output 'actual_product_reader: open-writer/partial-tail/completed-tail PASS'
} finally { $writer.Dispose() }
# Test-owned diagnostic input remains in ignored uat; never remove unknown data.
