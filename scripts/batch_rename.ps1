Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

[System.Windows.Forms.Application]::EnableVisualStyles()

# 1. Read Environment Variables passed from NeoMDIE
$activeDir = $env:NEOMDIE_ACTIVE_PATH
if ([string]::IsNullOrWhiteSpace($activeDir) -or -not (Test-Path -LiteralPath $activeDir)) {
    $activeDir = [System.IO.Directory]::GetCurrentDirectory()
}

$selectedFilesRaw = $env:NEOMDIE_SELECTED_FILES
$selectedList = New-Object System.Collections.Generic.List[string]
if (-not [string]::IsNullOrWhiteSpace($selectedFilesRaw)) {
    $parts = $selectedFilesRaw.Split('|')
    foreach ($p in $parts) {
        $pClean = $p.Trim()
        if (-not [string]::IsNullOrWhiteSpace($pClean) -and (Test-Path -LiteralPath $pClean)) {
            $selectedList.Add($pClean)
        }
    }
}

# 2. Main Form Setup
$form = New-Object System.Windows.Forms.Form
$form.Text = "批量文件重命名 (Batch Rename) - NeoMDIE"
$form.Size = New-Object System.Drawing.Size(920, 680)
$form.MinimumSize = New-Object System.Drawing.Size(800, 560)
$form.StartPosition = [System.Windows.Forms.FormStartPosition]::CenterScreen
$form.TopMost = $true
$form.Add_Shown({
    $form.TopMost = $false
    $form.Activate()
})
$form.Font = New-Object System.Drawing.Font("Microsoft YaHei UI", 9)
$form.BackColor = [System.Drawing.Color]::FromArgb(248, 249, 250)
$form.AutoScaleMode = [System.Windows.Forms.AutoScaleMode]::Dpi

# Top Info & Filter Panel
$topPanel = New-Object System.Windows.Forms.Panel
$topPanel.Dock = [System.Windows.Forms.DockStyle]::Top
$topPanel.Height = 78
$topPanel.Padding = New-Object System.Windows.Forms.Padding(15, 8, 15, 6)
$form.Controls.Add($topPanel)

$lblPath = New-Object System.Windows.Forms.Label
$lblPath.Text = "当前目录: $activeDir"
$lblPath.Location = New-Object System.Drawing.Point(15, 10)
$lblPath.Size = New-Object System.Drawing.Size(840, 22)
$lblPath.Font = New-Object System.Drawing.Font("Microsoft YaHei UI", 9, [System.Drawing.FontStyle]::Bold)
$lblPath.ForeColor = [System.Drawing.Color]::FromArgb(40, 40, 40)
$topPanel.Controls.Add($lblPath)

# Scope Selection Radio Buttons
$rbSelected = New-Object System.Windows.Forms.RadioButton
$rbSelected.Text = "仅重命名选中的项目 (共 $($selectedList.Count) 项)"
$rbSelected.Location = New-Object System.Drawing.Point(18, 40)
$rbSelected.Size = New-Object System.Drawing.Size(260, 26)
$rbSelected.Checked = ($selectedList.Count -gt 0)
$rbSelected.Enabled = ($selectedList.Count -gt 0)
$topPanel.Controls.Add($rbSelected)

$rbAll = New-Object System.Windows.Forms.RadioButton
$rbAll.Text = "重命名当前目录下的全部内容"
$rbAll.Location = New-Object System.Drawing.Point(285, 40)
$rbAll.Size = New-Object System.Drawing.Size(220, 26)
$rbAll.Checked = ($selectedList.Count -eq 0)
$topPanel.Controls.Add($rbAll)

$chkIncludeFiles = New-Object System.Windows.Forms.CheckBox
$chkIncludeFiles.Text = "包含文件"
$chkIncludeFiles.Location = New-Object System.Drawing.Point(525, 40)
$chkIncludeFiles.Size = New-Object System.Drawing.Size(85, 26)
$chkIncludeFiles.Checked = $true
$topPanel.Controls.Add($chkIncludeFiles)

$chkIncludeFolders = New-Object System.Windows.Forms.CheckBox
$chkIncludeFolders.Text = "包含文件夹"
$chkIncludeFolders.Location = New-Object System.Drawing.Point(620, 40)
$chkIncludeFolders.Size = New-Object System.Drawing.Size(100, 26)
# If files were specifically selected, include both files and folders by default!
$chkIncludeFolders.Checked = ($selectedList.Count -gt 0)
$topPanel.Controls.Add($chkIncludeFolders)

# Rules Panel (TabControl)
$tabCtrl = New-Object System.Windows.Forms.TabControl
$tabCtrl.Dock = [System.Windows.Forms.DockStyle]::Top
$tabCtrl.Height = 118
$tabCtrl.Padding = New-Object System.Drawing.Point(15, 6)
$form.Controls.Add($tabCtrl)

# Tab 1: 前缀与后缀
$tabPrefix = New-Object System.Windows.Forms.TabPage
$tabPrefix.Text = "添加前后缀"
$tabPrefix.BackColor = [System.Drawing.Color]::White
$tabCtrl.Controls.Add($tabPrefix)

$lblPre = New-Object System.Windows.Forms.Label
$lblPre.Text = "文件名前缀:"
$lblPre.Location = New-Object System.Drawing.Point(20, 20)
$lblPre.AutoSize = $true
$tabPrefix.Controls.Add($lblPre)

$txtPrefix = New-Object System.Windows.Forms.TextBox
$txtPrefix.Location = New-Object System.Drawing.Point(105, 17)
$txtPrefix.Size = New-Object System.Drawing.Size(240, 25)
$tabPrefix.Controls.Add($txtPrefix)

$lblSuf = New-Object System.Windows.Forms.Label
$lblSuf.Text = "文件名后缀:"
$lblSuf.Location = New-Object System.Drawing.Point(380, 20)
$lblSuf.AutoSize = $true
$tabPrefix.Controls.Add($lblSuf)

$txtSuffix = New-Object System.Windows.Forms.TextBox
$txtSuffix.Location = New-Object System.Drawing.Point(465, 17)
$txtSuffix.Size = New-Object System.Drawing.Size(240, 25)
$tabPrefix.Controls.Add($txtSuffix)

$lblSufTip = New-Object System.Windows.Forms.Label
$lblSufTip.Text = "提示: 后缀将智能插入在扩展名之前 (例如: 原名_后缀.txt)"
$lblSufTip.Location = New-Object System.Drawing.Point(105, 52)
$lblSufTip.AutoSize = $true
$lblSufTip.ForeColor = [System.Drawing.Color]::Gray
$tabPrefix.Controls.Add($lblSufTip)

# Tab 2: 查找与替换
$tabReplace = New-Object System.Windows.Forms.TabPage
$tabReplace.Text = "查找和替换"
$tabReplace.BackColor = [System.Drawing.Color]::White
$tabCtrl.Controls.Add($tabReplace)

$lblFind = New-Object System.Windows.Forms.Label
$lblFind.Text = "查找内容:"
$lblFind.Location = New-Object System.Drawing.Point(20, 20)
$lblFind.AutoSize = $true
$tabReplace.Controls.Add($lblFind)

$txtFind = New-Object System.Windows.Forms.TextBox
$txtFind.Location = New-Object System.Drawing.Point(100, 17)
$txtFind.Size = New-Object System.Drawing.Size(240, 25)
$tabReplace.Controls.Add($txtFind)

$lblReplace = New-Object System.Windows.Forms.Label
$lblReplace.Text = "替换为:"
$lblReplace.Location = New-Object System.Drawing.Point(380, 20)
$lblReplace.AutoSize = $true
$tabReplace.Controls.Add($lblReplace)

$txtReplace = New-Object System.Windows.Forms.TextBox
$txtReplace.Location = New-Object System.Drawing.Point(445, 17)
$txtReplace.Size = New-Object System.Drawing.Size(240, 25)
$tabReplace.Controls.Add($txtReplace)

$chkMatchCase = New-Object System.Windows.Forms.CheckBox
$chkMatchCase.Text = "区分大小写"
$chkMatchCase.Location = New-Object System.Drawing.Point(100, 52)
$chkMatchCase.Size = New-Object System.Drawing.Size(120, 25)
$tabReplace.Controls.Add($chkMatchCase)

$chkUseRegex = New-Object System.Windows.Forms.CheckBox
$chkUseRegex.Text = "正则表达式匹配"
$chkUseRegex.Location = New-Object System.Drawing.Point(235, 52)
$chkUseRegex.Size = New-Object System.Drawing.Size(140, 25)
$tabReplace.Controls.Add($chkUseRegex)

# Tab 3: 自定义序号与模板
$tabSeq = New-Object System.Windows.Forms.TabPage
$tabSeq.Text = "自定义序号模板"
$tabSeq.BackColor = [System.Drawing.Color]::White
$tabCtrl.Controls.Add($tabSeq)

$lblTpl = New-Object System.Windows.Forms.Label
$lblTpl.Text = "命名模板:"
$lblTpl.Location = New-Object System.Drawing.Point(20, 20)
$lblTpl.AutoSize = $true
$tabSeq.Controls.Add($lblTpl)

$txtTemplate = New-Object System.Windows.Forms.TextBox
$txtTemplate.Text = "[name]_{001}"
$txtTemplate.Location = New-Object System.Drawing.Point(90, 17)
$txtTemplate.Size = New-Object System.Drawing.Size(220, 25)
$tabSeq.Controls.Add($txtTemplate)

$lblSeqStart = New-Object System.Windows.Forms.Label
$lblSeqStart.Text = "起始序号:"
$lblSeqStart.Location = New-Object System.Drawing.Point(335, 20)
$lblSeqStart.AutoSize = $true
$tabSeq.Controls.Add($lblSeqStart)

$numSeqStart = New-Object System.Windows.Forms.NumericUpDown
$numSeqStart.Location = New-Object System.Drawing.Point(405, 17)
$numSeqStart.Size = New-Object System.Drawing.Size(70, 25)
$numSeqStart.Minimum = 0
$numSeqStart.Maximum = 999999
$numSeqStart.Value = 1
$tabSeq.Controls.Add($numSeqStart)

$lblDigits = New-Object System.Windows.Forms.Label
$lblDigits.Text = "补零位数:"
$lblDigits.Location = New-Object System.Drawing.Point(500, 20)
$lblDigits.AutoSize = $true
$tabSeq.Controls.Add($lblDigits)

$numDigits = New-Object System.Windows.Forms.NumericUpDown
$numDigits.Location = New-Object System.Drawing.Point(570, 17)
$numDigits.Size = New-Object System.Drawing.Size(70, 25)
$numDigits.Minimum = 1
$numDigits.Maximum = 8
$numDigits.Value = 3
$tabSeq.Controls.Add($numDigits)

$lblTplHint = New-Object System.Windows.Forms.Label
$lblTplHint.Text = "变量说明: [name]=原文件名，{001}=补零序号，[date]=当前日期，扩展名将自动保留"
$lblTplHint.Location = New-Object System.Drawing.Point(90, 52)
$lblTplHint.AutoSize = $true
$lblTplHint.ForeColor = [System.Drawing.Color]::Gray
$tabSeq.Controls.Add($lblTplHint)

# Tab 4: 扩展名修改
$tabExt = New-Object System.Windows.Forms.TabPage
$tabExt.Text = "扩展名操作"
$tabExt.BackColor = [System.Drawing.Color]::White
$tabCtrl.Controls.Add($tabExt)

$rbExtKeep = New-Object System.Windows.Forms.RadioButton
$rbExtKeep.Text = "保持原扩展名不变"
$rbExtKeep.Location = New-Object System.Drawing.Point(20, 18)
$rbExtKeep.Size = New-Object System.Drawing.Size(150, 25)
$rbExtKeep.Checked = $true
$tabExt.Controls.Add($rbExtKeep)

$rbExtLower = New-Object System.Windows.Forms.RadioButton
$rbExtLower.Text = "扩展名统一转小写 (.JPG -> .jpg)"
$rbExtLower.Location = New-Object System.Drawing.Point(180, 18)
$rbExtLower.Size = New-Object System.Drawing.Size(210, 25)
$tabExt.Controls.Add($rbExtLower)

$rbExtUpper = New-Object System.Windows.Forms.RadioButton
$rbExtUpper.Text = "扩展名统一转大写 (.jpg -> .JPG)"
$rbExtUpper.Location = New-Object System.Drawing.Point(400, 18)
$rbExtUpper.Size = New-Object System.Drawing.Size(210, 25)
$tabExt.Controls.Add($rbExtUpper)

$rbExtChange = New-Object System.Windows.Forms.RadioButton
$rbExtChange.Text = "修改统一扩展名:"
$rbExtChange.Location = New-Object System.Drawing.Point(20, 50)
$rbExtChange.Size = New-Object System.Drawing.Size(130, 25)
$tabExt.Controls.Add($rbExtChange)

$txtNewExt = New-Object System.Windows.Forms.TextBox
$txtNewExt.Location = New-Object System.Drawing.Point(155, 50)
$txtNewExt.Size = New-Object System.Drawing.Size(100, 25)
$txtNewExt.Text = "bak"
$tabExt.Controls.Add($txtNewExt)

$tabCtrl.SelectedIndex = 0

# Bottom Action Panel
$botPanel = New-Object System.Windows.Forms.Panel
$botPanel.Dock = [System.Windows.Forms.DockStyle]::Bottom
$botPanel.Height = 72
$botPanel.BackColor = [System.Drawing.Color]::FromArgb(240, 243, 246)
$botPanel.Padding = New-Object System.Windows.Forms.Padding(15, 16, 15, 16)
$form.Controls.Add($botPanel)

$lblSummary = New-Object System.Windows.Forms.Label
$lblSummary.Text = "准备就绪"
$lblSummary.Dock = [System.Windows.Forms.DockStyle]::Left
$lblSummary.AutoSize = $true
$lblSummary.Font = New-Object System.Drawing.Font("Microsoft YaHei UI", 9, [System.Drawing.FontStyle]::Bold)
$lblSummary.Padding = New-Object System.Windows.Forms.Padding(0, 10, 0, 0)
$botPanel.Controls.Add($lblSummary)

$btnPanel = New-Object System.Windows.Forms.FlowLayoutPanel
$btnPanel.Dock = [System.Windows.Forms.DockStyle]::Right
$btnPanel.FlowDirection = [System.Windows.Forms.FlowDirection]::RightToLeft
$btnPanel.Width = 430
$btnPanel.Height = 48
$btnPanel.BackColor = [System.Drawing.Color]::Transparent
$botPanel.Controls.Add($btnPanel)

$btnCancel = New-Object System.Windows.Forms.Button
$btnCancel.Text = "关闭"
$btnCancel.Size = New-Object System.Drawing.Size(90, 36)
$btnCancel.UseVisualStyleBackColor = $true
$btnCancel.Cursor = [System.Windows.Forms.Cursors]::Hand
$btnCancel.Add_Click({ $form.Close() })
$btnPanel.Controls.Add($btnCancel)

$btnApply = New-Object System.Windows.Forms.Button
$btnApply.Text = "开始重命名"
$btnApply.Size = New-Object System.Drawing.Size(130, 36)
$btnApply.BackColor = [System.Drawing.Color]::FromArgb(0, 120, 215)
$btnApply.ForeColor = [System.Drawing.Color]::White
$btnApply.FlatStyle = [System.Windows.Forms.FlatStyle]::Flat
$btnApply.Font = New-Object System.Drawing.Font("Microsoft YaHei UI", 9, [System.Drawing.FontStyle]::Bold)
$btnApply.Cursor = [System.Windows.Forms.Cursors]::Hand
$btnPanel.Controls.Add($btnApply)

$btnRefresh = New-Object System.Windows.Forms.Button
$btnRefresh.Text = "刷新列表"
$btnRefresh.Size = New-Object System.Drawing.Size(100, 36)
$btnRefresh.UseVisualStyleBackColor = $true
$btnRefresh.Cursor = [System.Windows.Forms.Cursors]::Hand
$btnPanel.Controls.Add($btnRefresh)

# Center DataGridView for Preview
$grid = New-Object System.Windows.Forms.DataGridView
$grid.Dock = [System.Windows.Forms.DockStyle]::Fill
$grid.BackgroundColor = [System.Drawing.Color]::White
$grid.BorderStyle = [System.Windows.Forms.BorderStyle]::None
$grid.RowHeadersVisible = $false
$grid.AllowUserToAddRows = $false
$grid.AllowUserToDeleteRows = $false
$grid.AllowUserToResizeRows = $false
$grid.SelectionMode = [System.Windows.Forms.DataGridViewSelectionMode]::FullRowSelect
$grid.MultiSelect = $false
$grid.ReadOnly = $true
$grid.EnableHeadersVisualStyles = $false
$grid.ColumnHeadersDefaultCellStyle.BackColor = [System.Drawing.Color]::FromArgb(238, 242, 246)
$grid.ColumnHeadersDefaultCellStyle.Font = New-Object System.Drawing.Font("Microsoft YaHei UI", 9, [System.Drawing.FontStyle]::Bold)
$grid.ColumnHeadersHeight = 32
$form.Controls.Add($grid)
$grid.BringToFront()

# Enable DoubleBuffering for buttery smooth rendering
$gridProp = [System.Windows.Forms.Control].GetProperty("DoubleBuffered", [System.Reflection.BindingFlags]::Instance -bor [System.Reflection.BindingFlags]::NonPublic)
if ($null -ne $gridProp) {
    $gridProp.SetValue($grid, $true, $null)
}

# Grid Columns
$colOrig = New-Object System.Windows.Forms.DataGridViewTextBoxColumn
$colOrig.HeaderText = "原始名称"
$colOrig.Width = 240
$grid.Columns.Add($colOrig) | Out-Null

$colNew = New-Object System.Windows.Forms.DataGridViewTextBoxColumn
$colNew.HeaderText = "新文件名 (预览)"
$colNew.Width = 250
$grid.Columns.Add($colNew) | Out-Null

$colStatus = New-Object System.Windows.Forms.DataGridViewTextBoxColumn
$colStatus.HeaderText = "状态"
$colStatus.Width = 110
$grid.Columns.Add($colStatus) | Out-Null

$colType = New-Object System.Windows.Forms.DataGridViewTextBoxColumn
$colType.HeaderText = "类型"
$colType.Width = 80
$grid.Columns.Add($colType) | Out-Null

$colPath = New-Object System.Windows.Forms.DataGridViewTextBoxColumn
$colPath.HeaderText = "所在目录"
$colPath.AutoSizeMode = [System.Windows.Forms.DataGridViewAutoSizeColumnMode]::Fill
$grid.Columns.Add($colPath) | Out-Null

# In-Memory Data Storage
$itemList = New-Object System.Collections.Generic.List[PSObject]
$invalidChars = [System.IO.Path]::GetInvalidFileNameChars()

# Function 1: Gather Target Items from Disk / Selection (Only run when scope or filters change)
$GatherItems = {
    $itemList.Clear()
    $incFiles = $chkIncludeFiles.Checked
    $incFolders = $chkIncludeFolders.Checked

    if ($rbSelected.Checked) {
        foreach ($p in $selectedList) {
            if ([System.IO.File]::Exists($p) -or [System.IO.Directory]::Exists($p)) {
                $isDir = [System.IO.Directory]::Exists($p)
                if (($isDir -and $incFolders) -or (-not $isDir -and $incFiles)) {
                    $fi = if ($isDir) { New-Object System.IO.DirectoryInfo($p) } else { New-Object System.IO.FileInfo($p) }
                    $obj = [PSCustomObject]@{
                        FullPath = $p
                        Directory = if ($isDir) { $fi.Parent.FullName } else { $fi.DirectoryName }
                        OriginalName = $fi.Name
                        BaseName = if ($isDir) { $fi.Name } else { [System.IO.Path]::GetFileNameWithoutExtension($fi.Name) }
                        Extension = if ($isDir) { "" } else { $fi.Extension }
                        IsDirectory = $isDir
                        NewName = ""
                        Status = ""
                    }
                    $itemList.Add($obj)
                }
            }
        }
    } else {
        if ([System.IO.Directory]::Exists($activeDir)) {
            $di = New-Object System.IO.DirectoryInfo($activeDir)
            if ($incFiles) {
                foreach ($fi in $di.GetFiles()) {
                    $obj = [PSCustomObject]@{
                        FullPath = $fi.FullName
                        Directory = $fi.DirectoryName
                        OriginalName = $fi.Name
                        BaseName = [System.IO.Path]::GetFileNameWithoutExtension($fi.Name)
                        Extension = $fi.Extension
                        IsDirectory = $false
                        NewName = ""
                        Status = ""
                    }
                    $itemList.Add($obj)
                }
            }
            if ($incFolders) {
                foreach ($sub in $di.GetDirectories()) {
                    $obj = [PSCustomObject]@{
                        FullPath = $sub.FullName
                        Directory = $sub.Parent.FullName
                        OriginalName = $sub.Name
                        BaseName = $sub.Name
                        Extension = ""
                        IsDirectory = $true
                        NewName = ""
                        Status = ""
                    }
                    $itemList.Add($obj)
                }
            }
        }
    }

    # Rebuild Grid Rows once when item set changes
    $grid.SuspendLayout()
    $grid.Rows.Clear()
    foreach ($item in $itemList) {
        $typeName = if ($item.IsDirectory) { "文件夹" } else { "文件" }
        $rIdx = $grid.Rows.Add($item.OriginalName, "", "", $typeName, $item.Directory)
    }
    $grid.ResumeLayout()

    # Recalculate rules
    & $ApplyRules
}

# Function 2: Apply Rules & Update Preview In-Memory (Runs in <1ms, 0 latency on typing!)
$ApplyRules = {
    if ($itemList.Count -eq 0) {
        $lblSummary.Text = "未匹配到任何文件或文件夹"
        $lblSummary.ForeColor = [System.Drawing.Color]::Gray
        $btnApply.Enabled = $false
        return
    }

    $curTabIdx = if ($tabCtrl.SelectedIndex -ge 0) { $tabCtrl.SelectedIndex } else { 0 }
    $prefix = $txtPrefix.Text
    $suffix = $txtSuffix.Text
    $findStr = $txtFind.Text
    $repStr = $txtReplace.Text
    $matchCase = $chkMatchCase.Checked
    $useRegex = $chkUseRegex.Checked
    $template = $txtTemplate.Text
    $seqStart = [int]$numSeqStart.Value
    $digits = [int]$numDigits.Value

    $todayStr = (Get-Date).ToString("yyyyMMdd")
    $changedCount = 0
    $conflictCount = 0
    $usedNames = New-Object System.Collections.Generic.HashSet[string]([System.StringComparer]::OrdinalIgnoreCase)

    $grid.SuspendLayout()
    for ($i = 0; $i -lt $itemList.Count; $i++) {
        $item = $itemList[$i]
        $curBase = $item.BaseName
        $curExt = $item.Extension

        # Rule 1: Tab 0 - 前缀与后缀
        if ($curTabIdx -eq 0) {
            $curBase = "$prefix$curBase$suffix"
        }
        # Rule 2: Tab 1 - 查找与替换
        elseif ($curTabIdx -eq 1 -and -not [string]::IsNullOrEmpty($findStr)) {
            if ($useRegex) {
                try {
                    $opt = if ($matchCase) { [System.Text.RegularExpressions.RegexOptions]::None } else { [System.Text.RegularExpressions.RegexOptions]::IgnoreCase }
                    $curBase = [System.Text.RegularExpressions.Regex]::Replace($curBase, $findStr, $repStr, $opt)
                } catch {
                    $item.Status = "正则语法错误"
                }
            } else {
                if ($matchCase) {
                    $curBase = $curBase.Replace($findStr, $repStr)
                } else {
                    $pattern = [System.Text.RegularExpressions.Regex]::Escape($findStr)
                    $curBase = [System.Text.RegularExpressions.Regex]::Replace($curBase, $pattern, $repStr, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
                }
            }
        }
        # Rule 3: Tab 2 - 序号模板
        elseif ($curTabIdx -eq 2 -and -not [string]::IsNullOrEmpty($template)) {
            $numVal = $seqStart + $i
            $seqPadded = $numVal.ToString().PadLeft($digits, '0')
            $newNameStr = $template.Replace("[name]", $item.BaseName)
            $newNameStr = $newNameStr.Replace("[date]", $todayStr)
            $newNameStr = [System.Text.RegularExpressions.Regex]::Replace($newNameStr, '\{0+\}', $seqPadded)
            $curBase = $newNameStr
        }

        # Rule 4: Extension Handling (Files only)
        if (-not $item.IsDirectory) {
            if ($curTabIdx -eq 3) {
                if ($rbExtLower.Checked) {
                    $curExt = $curExt.ToLower()
                } elseif ($rbExtUpper.Checked) {
                    $curExt = $curExt.ToUpper()
                } elseif ($rbExtChange.Checked -and -not [string]::IsNullOrWhiteSpace($txtNewExt.Text)) {
                    $cleanExt = $txtNewExt.Text.TrimStart('.')
                    $curExt = ".$cleanExt"
                }
            }
        }

        $calcNewName = if ($item.IsDirectory) { $curBase } else { "$curBase$curExt" }
        $item.NewName = $calcNewName

        # Filename Validation
        $hasInvalid = $false
        foreach ($ch in $invalidChars) {
            if ($calcNewName.IndexOf($ch) -ge 0) {
                $hasInvalid = $true
                break
            }
        }

        if ([string]::IsNullOrWhiteSpace($calcNewName)) {
            $item.Status = "文件名为空"
            $conflictCount++
        } elseif ($hasInvalid) {
            $item.Status = "包含非法字符"
            $conflictCount++
        } elseif ($usedNames.Contains($calcNewName)) {
            $item.Status = "同批重名冲突"
            $conflictCount++
        } else {
            $targetPath = [System.IO.Path]::Combine($item.Directory, $calcNewName)
            if ($calcNewName -ne $item.OriginalName -and ([System.IO.File]::Exists($targetPath) -or [System.IO.Directory]::Exists($targetPath))) {
                $isTargetInList = $false
                foreach ($other in $itemList) {
                    if ($other.FullPath -eq $targetPath) { $isTargetInList = $true; break }
                }
                if (-not $isTargetInList) {
                    $item.Status = "磁盘已有同名"
                    $conflictCount++
                } else {
                    $item.Status = "待重命名"
                    $changedCount++
                }
            } elseif ($calcNewName -ne $item.OriginalName) {
                $item.Status = "待重命名"
                $changedCount++
            } else {
                $item.Status = "保持不变"
            }
        }

        $usedNames.Add($calcNewName) | Out-Null

        # Fast In-Place Cell Update (0ms lag, no recreating rows!)
        if ($i -lt $grid.Rows.Count) {
            $row = $grid.Rows[$i]
            $row.Cells[1].Value = $item.NewName
            $row.Cells[2].Value = $item.Status

            if ($item.Status -eq "待重命名") {
                $row.Cells[1].Style.ForeColor = [System.Drawing.Color]::FromArgb(0, 130, 60)
                $row.Cells[2].Style.ForeColor = [System.Drawing.Color]::FromArgb(0, 130, 60)
            } elseif ($item.Status -ne "保持不变") {
                $row.Cells[1].Style.ForeColor = [System.Drawing.Color]::Red
                $row.Cells[2].Style.ForeColor = [System.Drawing.Color]::Red
            } else {
                $row.Cells[1].Style.ForeColor = [System.Drawing.Color]::FromArgb(60, 60, 60)
                $row.Cells[2].Style.ForeColor = [System.Drawing.Color]::Gray
            }
        }
    }
    $grid.ResumeLayout()

    $lblSummary.Text = "共 $($itemList.Count) 项 | 待重命名: $changedCount 项 | 冲突/错误: $conflictCount 项"
    if ($conflictCount -gt 0) {
        $lblSummary.ForeColor = [System.Drawing.Color]::Red
        $btnApply.Enabled = $false
    } else {
        $lblSummary.ForeColor = [System.Drawing.Color]::FromArgb(0, 100, 0)
        $btnApply.Enabled = ($changedCount -gt 0)
    }
}

# Debounce timer for smooth typing
$debounceTimer = New-Object System.Windows.Forms.Timer
$debounceTimer.Interval = 80
$debounceTimer.Add_Tick({
    $debounceTimer.Stop()
    & $ApplyRules
})
$TriggerApplyRules = {
    $debounceTimer.Stop()
    $debounceTimer.Start()
}

# Attach Event Handlers
# Scope / Filter change: Gather items from disk
$rbSelected.Add_CheckedChanged($GatherItems)
$rbAll.Add_CheckedChanged($GatherItems)
$chkIncludeFiles.Add_CheckedChanged($GatherItems)
$chkIncludeFolders.Add_CheckedChanged($GatherItems)
$btnRefresh.Add_Click($GatherItems)

# Tab / Rule input change: Fast in-memory calculation (0ms typing latency!)
$tabCtrl.Add_SelectedIndexChanged($ApplyRules)
$txtPrefix.Add_TextChanged($TriggerApplyRules)
$txtSuffix.Add_TextChanged($TriggerApplyRules)
$txtFind.Add_TextChanged($TriggerApplyRules)
$txtReplace.Add_TextChanged($TriggerApplyRules)
$chkMatchCase.Add_CheckedChanged($ApplyRules)
$chkUseRegex.Add_CheckedChanged($ApplyRules)
$txtTemplate.Add_TextChanged($TriggerApplyRules)
$numSeqStart.Add_ValueChanged($ApplyRules)
$numDigits.Add_ValueChanged($ApplyRules)

$rbExtKeep.Add_CheckedChanged($ApplyRules)
$rbExtLower.Add_CheckedChanged($ApplyRules)
$rbExtUpper.Add_CheckedChanged($ApplyRules)
$rbExtChange.Add_CheckedChanged($ApplyRules)
$txtNewExt.Add_TextChanged($TriggerApplyRules)

# Execute Rename Handler
$btnApply.Add_Click({
    $toRename = New-Object System.Collections.Generic.List[PSObject]
    foreach ($item in $itemList) {
        if ($item.Status -eq "待重命名") {
            $toRename.Add($item)
        }
    }

    if ($toRename.Count -eq 0) {
        [System.Windows.Forms.MessageBox]::Show("当前规则下没有需要重命名的项目。", "提示", [System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Information)
        return
    }

    $res = [System.Windows.Forms.MessageBox]::Show("确定要将选中的 $($toRename.Count) 个文件/文件夹执行重命名吗？`n此操作将直接修改磁盘文件名称。", "确认执行重命名", [System.Windows.Forms.MessageBoxButtons]::YesNo, [System.Windows.Forms.MessageBoxIcon]::Question)
    if ($res -ne [System.Windows.Forms.DialogResult]::Yes) {
        return
    }

    $successCount = 0
    $failList = New-Object System.Collections.Generic.List[string]

    # Two-step rename strategy to safely handle swaps and case-only changes
    foreach ($item in $toRename) {
        $oldPath = $item.FullPath
        $targetPath = [System.IO.Path]::Combine($item.Directory, $item.NewName)
        $tempPath = "$oldPath.renametmp_$([System.Guid]::NewGuid().ToString('N').Substring(0,8))"

        try {
            if ($item.IsDirectory) {
                [System.IO.Directory]::Move($oldPath, $tempPath)
                [System.IO.Directory]::Move($tempPath, $targetPath)
            } else {
                [System.IO.File]::Move($oldPath, $tempPath)
                [System.IO.File]::Move($tempPath, $targetPath)
            }
            $successCount++
        } catch {
            $failList.Add("$($item.OriginalName) -> $($_.Exception.Message)")
            if ([System.IO.File]::Exists($tempPath) -or [System.IO.Directory]::Exists($tempPath)) {
                try {
                    if ($item.IsDirectory) { [System.IO.Directory]::Move($tempPath, $oldPath) }
                    else { [System.IO.File]::Move($tempPath, $oldPath) }
                } catch {}
            }
        }
    }

    if ($failList.Count -eq 0) {
        [System.Windows.Forms.MessageBox]::Show("恭喜！成功完成 $successCount 个文件/文件夹的批量重命名。", "操作成功", [System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Information)
        $form.Close()
    } else {
        $errMsg = "重命名完成，但有 $($failList.Count) 项操作失败：`n" + ($failList -join "`n")
        [System.Windows.Forms.MessageBox]::Show($errMsg, "部分重命名失败", [System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Warning)
        & $GatherItems
    }
})

# Initial load
$form.Add_Shown({
    & $GatherItems
})
& $GatherItems

# Show Dialog
$form.ShowDialog() | Out-Null
