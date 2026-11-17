Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

[System.Windows.Forms.Application]::EnableVisualStyles()

# 1. 读取 NeoMDIE 传入的环境变量
$activeDir = $env:NEOMDIE_ACTIVE_PATH
$paneCountRaw = $env:NEOMDIE_PANE_COUNT
$paneCount = 1
if (-not [string]::IsNullOrWhiteSpace($paneCountRaw)) {
    [int]::TryParse($paneCountRaw, [ref]$paneCount) | Out-Null
}

$paneList = New-Object System.Collections.Generic.List[string]
if ($paneCount -gt 0) {
    for ($i = 0; $i -lt $paneCount; $i++) {
        $pVal = [Environment]::GetEnvironmentVariable("NEOMDIE_PANE_$i")
        if (-not [string]::IsNullOrWhiteSpace($pVal) -and (Test-Path -LiteralPath $pVal)) {
            $paneList.Add($pVal)
        }
    }
}

if ($paneList.Count -lt 2) {
    # 如果当前标签页只有一个栏，尝试取当前目录作为基准，提示用户或者让用户选择对比目录
    if ([string]::IsNullOrWhiteSpace($activeDir) -or -not (Test-Path -LiteralPath $activeDir)) {
        $activeDir = [System.IO.Directory]::GetCurrentDirectory()
    }
    if ($paneList.Count -eq 0 -and (Test-Path -LiteralPath $activeDir)) {
        $paneList.Add($activeDir)
    }
}

$dirA = if ($paneList.Count -ge 1) { $paneList[0] } else { $activeDir }
$dirB = if ($paneList.Count -ge 2) { $paneList[1] } else { "" }

# 2. 创建主窗体
$form = New-Object System.Windows.Forms.Form
$form.Text = "多分栏文件差异对比 (Compare Panes) - NeoMDIE"
$form.Size = New-Object System.Drawing.Size(1080, 720)
$form.MinimumSize = New-Object System.Drawing.Size(860, 560)
$form.StartPosition = [System.Windows.Forms.FormStartPosition]::CenterScreen
$form.TopMost = $true
$form.Add_Shown({
    $form.TopMost = $false
    $form.Activate()
})
$form.Font = New-Object System.Drawing.Font("Microsoft YaHei UI", 9)
$form.BackColor = [System.Drawing.Color]::FromArgb(248, 249, 250)
$form.AutoScaleMode = [System.Windows.Forms.AutoScaleMode]::Dpi

# 中间主数据列表 (ListView 先添加，避免被 Dock=Top 的 Panel 遮挡顶部)
$listView = New-Object System.Windows.Forms.ListView
$listView.Dock = [System.Windows.Forms.DockStyle]::Fill
$listView.View = [System.Windows.Forms.View]::Details
$listView.FullRowSelect = $true
$listView.GridLines = $true
$listView.HideSelection = $false
$listView.Font = New-Object System.Drawing.Font("Microsoft YaHei UI", 9)
$form.Controls.Add($listView)

# 添加列
$listView.Columns.Add("状态", 100) | Out-Null
$listView.Columns.Add("名称", 220) | Out-Null
$listView.Columns.Add("大小 [A]", 110) | Out-Null
$listView.Columns.Add("修改时间 [A]", 155) | Out-Null
$listView.Columns.Add("大小 [B]", 110) | Out-Null
$listView.Columns.Add("修改时间 [B]", 155) | Out-Null
$listView.Columns.Add("差异说明", 200) | Out-Null

# 顶部路径与选择栏
$topPanel = New-Object System.Windows.Forms.Panel
$topPanel.Dock = [System.Windows.Forms.DockStyle]::Top
$topPanel.Height = 120
$topPanel.Padding = New-Object System.Windows.Forms.Padding(16, 10, 16, 8)
$topPanel.BackColor = [System.Drawing.Color]::White
$form.Controls.Add($topPanel)

# 目录 A 选择行
$lblA = New-Object System.Windows.Forms.Label
$lblA.Text = "左侧/基准栏 [A]:"
$lblA.Location = New-Object System.Drawing.Point(16, 14)
$lblA.Size = New-Object System.Drawing.Size(110, 24)
$lblA.Font = New-Object System.Drawing.Font("Microsoft YaHei UI", 9, [System.Drawing.FontStyle]::Bold)
$topPanel.Controls.Add($lblA)

$txtA = New-Object System.Windows.Forms.TextBox
$txtA.Text = $dirA
$txtA.Location = New-Object System.Drawing.Point(130, 12)
$txtA.Size = New-Object System.Drawing.Size(780, 26)
$txtA.Anchor = [System.Windows.Forms.AnchorStyles]::Top -bor [System.Windows.Forms.AnchorStyles]::Left -bor [System.Windows.Forms.AnchorStyles]::Right
$topPanel.Controls.Add($txtA)

$btnBrowseA = New-Object System.Windows.Forms.Button
$btnBrowseA.Text = "浏览..."
$btnBrowseA.Location = New-Object System.Drawing.Point(920, 11)
$btnBrowseA.Size = New-Object System.Drawing.Size(75, 28)
$btnBrowseA.Anchor = [System.Windows.Forms.AnchorStyles]::Top -bor [System.Windows.Forms.AnchorStyles]::Right
$btnBrowseA.Add_Click({
    $fbd = New-Object System.Windows.Forms.FolderBrowserDialog
    $fbd.SelectedPath = $txtA.Text
    if ($fbd.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
        $txtA.Text = $fbd.SelectedPath
    }
})
$topPanel.Controls.Add($btnBrowseA)

# 目录 B 选择行
$lblB = New-Object System.Windows.Forms.Label
$lblB.Text = "右侧/对比栏 [B]:"
$lblB.Location = New-Object System.Drawing.Point(16, 48)
$lblB.Size = New-Object System.Drawing.Size(110, 24)
$lblB.Font = New-Object System.Drawing.Font("Microsoft YaHei UI", 9, [System.Drawing.FontStyle]::Bold)
$topPanel.Controls.Add($lblB)

$txtB = New-Object System.Windows.Forms.TextBox
$txtB.Text = $dirB
$txtB.Location = New-Object System.Drawing.Point(130, 46)
$txtB.Size = New-Object System.Drawing.Size(780, 26)
$txtB.Anchor = [System.Windows.Forms.AnchorStyles]::Top -bor [System.Windows.Forms.AnchorStyles]::Left -bor [System.Windows.Forms.AnchorStyles]::Right
$topPanel.Controls.Add($txtB)

$btnBrowseB = New-Object System.Windows.Forms.Button
$btnBrowseB.Text = "浏览..."
$btnBrowseB.Location = New-Object System.Drawing.Point(920, 45)
$btnBrowseB.Size = New-Object System.Drawing.Size(75, 28)
$btnBrowseB.Anchor = [System.Windows.Forms.AnchorStyles]::Top -bor [System.Windows.Forms.AnchorStyles]::Right
$btnBrowseB.Add_Click({
    $fbd = New-Object System.Windows.Forms.FolderBrowserDialog
    $fbd.SelectedPath = $txtB.Text
    if ($fbd.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
        $txtB.Text = $fbd.SelectedPath
    }
})
$topPanel.Controls.Add($btnBrowseB)

# 过滤与操作按钮行
$chkDiffOnly = New-Object System.Windows.Forms.CheckBox
$chkDiffOnly.Text = "仅显示存在差异的项目 (隐藏完全相同的文件)"
$chkDiffOnly.Checked = $false
$chkDiffOnly.Location = New-Object System.Drawing.Point(130, 82)
$chkDiffOnly.Size = New-Object System.Drawing.Size(320, 24)
$topPanel.Controls.Add($chkDiffOnly)

$btnCompare = New-Object System.Windows.Forms.Button
$btnCompare.Text = " 立即比对 / 刷新 "
$btnCompare.Location = New-Object System.Drawing.Point(460, 80)
$btnCompare.Size = New-Object System.Drawing.Size(130, 30)
$btnCompare.BackColor = [System.Drawing.Color]::FromArgb(0, 120, 215)
$btnCompare.ForeColor = [System.Drawing.Color]::White
$btnCompare.FlatStyle = [System.Windows.Forms.FlatStyle]::Flat
$btnCompare.Font = New-Object System.Drawing.Font("Microsoft YaHei UI", 9, [System.Drawing.FontStyle]::Bold)
$topPanel.Controls.Add($btnCompare)

# 底部状态栏
$statusStrip = New-Object System.Windows.Forms.StatusStrip
$statusLbl = New-Object System.Windows.Forms.ToolStripStatusLabel
$statusLbl.Text = "就绪。点击【立即比对】分析两栏文件差异。"
$statusStrip.Items.Add($statusLbl) | Out-Null
$form.Controls.Add($statusStrip)

function Format-FileSize($bytes) {
    if ($null -eq $bytes) { return "-" }
    if ($bytes -lt 1KB) { return "$bytes B" }
    if ($bytes -lt 1MB) { return "{0:N2} KB" -f ($bytes / 1KB) }
    if ($bytes -lt 1GB) { return "{0:N2} MB" -f ($bytes / 1MB) }
    return "{0:N2} GB" -f ($bytes / 1GB)
}

# 比对逻辑
$runCompare = {
    $pathA = $txtA.Text.Trim()
    $pathB = $txtB.Text.Trim()

    if ([string]::IsNullOrWhiteSpace($pathA) -or -not (Test-Path -LiteralPath $pathA)) {
        [System.Windows.Forms.MessageBox]::Show("左侧栏 [A] 目录不存在，请选择有效文件夹！", "提示", [System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Warning)
        return
    }
    if ([string]::IsNullOrWhiteSpace($pathB) -or -not (Test-Path -LiteralPath $pathB)) {
        [System.Windows.Forms.MessageBox]::Show("右侧栏 [B] 目录不存在，请选择有效文件夹！", "提示", [System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Warning)
        return
    }

    $listView.Items.Clear()
    $statusLbl.Text = "正在扫描对比目录，请稍候..."
    $form.Cursor = [System.Windows.Forms.Cursors]::WaitCursor
    $form.Update()

    try {
        $dictA = @{}
        Get-ChildItem -LiteralPath $pathA -Force -ErrorAction SilentlyContinue | ForEach-Object {
            $dictA[$_.Name] = $_
        }

        $dictB = @{}
        Get-ChildItem -LiteralPath $pathB -Force -ErrorAction SilentlyContinue | ForEach-Object {
            $dictB[$_.Name] = $_
        }

        $allNames = New-Object System.Collections.Generic.HashSet[string]
        foreach ($k in $dictA.Keys) { $allNames.Add($k) | Out-Null }
        foreach ($k in $dictB.Keys) { $allNames.Add($k) | Out-Null }

        $sortedNames = $allNames | Sort-Object

        $countSame = 0
        $countDiff = 0
        $countOnlyA = 0
        $countOnlyB = 0

        $filterDiff = $chkDiffOnly.Checked

        $listView.BeginUpdate()
        foreach ($name in $sortedNames) {
            $hasA = $dictA.ContainsKey($name)
            $hasB = $dictB.ContainsKey($name)

            $itemA = if ($hasA) { $dictA[$name] } else { $null }
            $itemB = if ($hasB) { $dictB[$name] } else { $null }

            $statusText = ""
            $diffText = ""
            $clrFg = [System.Drawing.Color]::Black
            $clrBg = [System.Drawing.Color]::White

            if ($hasA -and -not $hasB) {
                $statusText = "仅在 A 存在"
                $diffText = "右侧 [B] 缺失"
                $clrFg = [System.Drawing.Color]::FromArgb(0, 102, 204)
                $countOnlyA++
            }
            elseif (-not $hasA -and $hasB) {
                $statusText = "仅在 B 存在"
                $diffText = "左侧 [A] 缺失"
                $clrFg = [System.Drawing.Color]::FromArgb(153, 51, 0)
                $countOnlyB++
            }
            else {
                # 两边都有
                $isDirA = $itemA.PSIsContainer
                $isDirB = $itemB.PSIsContainer

                if ($isDirA -ne $isDirB) {
                    $statusText = "类型不同"
                    $diffText = if ($isDirA) { "A为目录，B为文件" } else { "A为文件，B为目录" }
                    $clrFg = [System.Drawing.Color]::Red
                    $countDiff++
                }
                elseif ($isDirA) {
                    $statusText = "同名文件夹"
                    $diffText = "目录"
                    $clrFg = [System.Drawing.Color]::FromArgb(100, 100, 100)
                    $countSame++
                }
                else {
                    # 文件比对
                    $sizeA = $itemA.Length
                    $sizeB = $itemB.Length
                    $timeA = $itemA.LastWriteTime
                    $timeB = $itemB.LastWriteTime

                    $sizeEqual = ($sizeA -eq $sizeB)
                    $timeDiffSec = [Math]::Abs(($timeA - $timeB).TotalSeconds)
                    $timeEqual = ($timeDiffSec -lt 2)

                    if ($sizeEqual -and $timeEqual) {
                        $statusText = "完全相同"
                        $diffText = "大小与修改时间一致"
                        $clrFg = [System.Drawing.Color]::FromArgb(46, 125, 50) # 绿色
                        $countSame++
                    }
                    else {
                        $diffParts = @()
                        if (-not $sizeEqual) {
                            $diffBytes = $sizeB - $sizeA
                            $sign = if ($diffBytes -gt 0) { "+$diffBytes B" } else { "$diffBytes B" }
                            $diffParts += "大小不同($sign)"
                        }
                        if (-not $timeEqual) {
                            if ($timeA -gt $timeB) {
                                $diffParts += "A更新"
                            } else {
                                $diffParts += "B更新"
                            }
                        }
                        $statusText = "存在差异"
                        $diffText = $diffParts -join ", "
                        $clrFg = [System.Drawing.Color]::FromArgb(198, 40, 40) # 红色差异
                        $countDiff++
                    }
                }
            }

            if ($filterDiff -and ($statusText -eq "完全相同" -or $statusText -eq "同名文件夹")) {
                continue
            }

            $sizeStrA = if ($itemA -and -not $itemA.PSIsContainer) { Format-FileSize($itemA.Length) } elseif ($itemA) { "<DIR>" } else { "-" }
            $timeStrA = if ($itemA) { $itemA.LastWriteTime.ToString("yyyy-MM-dd HH:mm:ss") } else { "-" }

            $sizeStrB = if ($itemB -and -not $itemB.PSIsContainer) { Format-FileSize($itemB.Length) } elseif ($itemB) { "<DIR>" } else { "-" }
            $timeStrB = if ($itemB) { $itemB.LastWriteTime.ToString("yyyy-MM-dd HH:mm:ss") } else { "-" }

            $lvItem = New-Object System.Windows.Forms.ListViewItem($statusText)
            $lvItem.ForeColor = $clrFg
            $lvItem.SubItems.Add($name) | Out-Null
            $lvItem.SubItems.Add($sizeStrA) | Out-Null
            $lvItem.SubItems.Add($timeStrA) | Out-Null
            $lvItem.SubItems.Add($sizeStrB) | Out-Null
            $lvItem.SubItems.Add($timeStrB) | Out-Null
            $lvItem.SubItems.Add($diffText) | Out-Null

            $listView.Items.Add($lvItem) | Out-Null
        }
        $listView.EndUpdate()

        $statusLbl.Text = "比对完成: 共 $($sortedNames.Count) 项 | 完全相同: $countSame 项 | 存在差异: $countDiff 项 | 仅A独有: $countOnlyA 项 | 仅B独有: $countOnlyB 项"
    }
    catch {
        $statusLbl.Text = "比对过程发生错误: " + $_.Exception.Message
    }
    finally {
        $form.Cursor = [System.Windows.Forms.Cursors]::Default
    }
}

$btnCompare.Add_Click($runCompare)
$chkDiffOnly.Add_CheckedChanged($runCompare)

# 自动初次比对
if (-not [string]::IsNullOrWhiteSpace($dirA) -and -not [string]::IsNullOrWhiteSpace($dirB) -and
    (Test-Path -LiteralPath $dirA) -and (Test-Path -LiteralPath $dirB)) {
    & $runCompare
}

[void]$form.ShowDialog()
