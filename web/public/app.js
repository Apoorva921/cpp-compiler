document.addEventListener('DOMContentLoaded', () => {
    const editor = document.getElementById('code-editor');
    const lineNumbers = document.getElementById('line-numbers');
    const charCount = document.getElementById('char-count');
    const runBtn = document.getElementById('btn-run');
    const runSpinner = document.getElementById('run-spinner');
    const runIcon = document.getElementById('run-icon');
    const exampleSelect = document.getElementById('example-select');
    const fileUpload = document.getElementById('file-upload');
    const dropZone = document.getElementById('drop-zone');
    const dropOverlay = document.getElementById('drop-overlay');
    const activeFilename = document.getElementById('active-filename');
    const statusDot = document.getElementById('status-indicator');

    // Outputs
    const consoleOutput = document.getElementById('console-output');
    const execStatus = document.getElementById('exec-status');
    const execTime = document.getElementById('exec-time');
    const execCode = document.getElementById('exec-code');
    const clearBtn = document.getElementById('btn-clear-output');

    // Friendly display names for example recipes
    const friendlyNames = {
        'test_sample.cpp': '🧮 Calculator OOP',
        '05_linked_list.cpp': '🚂 Dynamic Linked List',
        '02_fibonacci.cpp': '🌀 Fibonacci Generator',
        '03_pointers_arrays.cpp': '🪄 Pointer Swap Magic',
        '01_hello.cpp': '👋 Hello World',
        '04_classes_oop.cpp': '🎯 Point & Coordinates'
    };

    // 1. Line Numbers & Scroll Sync
    function updateLineNumbers() {
        const lines = editor.value.split('\n');
        const count = lines.length;
        charCount.textContent = `${count} line${count === 1 ? '' : 's'}`;

        let nums = '';
        for (let i = 1; i <= count; ++i) {
            nums += i + '\n';
        }
        lineNumbers.textContent = nums;
    }

    editor.addEventListener('input', () => {
        updateLineNumbers();
    });

    editor.addEventListener('scroll', () => {
        lineNumbers.scrollTop = editor.scrollTop;
    });

    // 2. Tab key handling & auto-indent
    editor.addEventListener('keydown', (e) => {
        if ((e.ctrlKey || e.metaKey) && e.key === 'Enter') {
            e.preventDefault();
            runCode();
            return;
        }

        if (e.key === 'Tab') {
            e.preventDefault();
            const start = editor.selectionStart;
            const end = editor.selectionEnd;
            editor.value = editor.value.substring(0, start) + '    ' + editor.value.substring(end);
            editor.selectionStart = editor.selectionEnd = start + 4;
            updateLineNumbers();
        }

        if (e.key === 'Enter') {
            const start = editor.selectionStart;
            const line = editor.value.substring(0, start).split('\n').pop();
            const indentMatch = line.match(/^(\s+)/);
            if (indentMatch) {
                e.preventDefault();
                const indent = indentMatch[1];
                const extra = line.trim().endsWith('{') ? '    ' : '';
                const insertion = '\n' + indent + extra;
                editor.value = editor.value.substring(0, start) + insertion + editor.value.substring(editor.selectionEnd);
                editor.selectionStart = editor.selectionEnd = start + insertion.length;
                updateLineNumbers();
            }
        }
    });

    // 3. Execution Logic
    async function runCode() {
        const code = editor.value;
        if (!code.trim()) {
            consoleOutput.textContent = 'Oops! Nothing to run. Type some C++ code first! 😊';
            return;
        }

        setRunningState(true);
        consoleOutput.textContent = '🚀 Compiling and running...\n';
        execStatus.textContent = '⚡ Running...';
        execStatus.className = 'playful-tag';

        try {
            const res = await fetch('/api/run', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ code })
            });
            const data = await res.json();

            setRunningState(false);
            execTime.textContent = `⚡ ${data.timeMs || 0} ms`;
            execCode.textContent = `Exit: ${data.exitCode}`;

            if (data.timedOut) {
                execStatus.textContent = '⏰ Timeout (8s)';
                execStatus.className = 'playful-tag error';
                consoleOutput.textContent = 'Program timed out! Looks like an infinite loop. 🔄';
                return;
            }

            if (data.success) {
                execStatus.textContent = '🎉 Success';
                execStatus.className = 'playful-tag success';
            } else {
                execStatus.textContent = '💥 Error';
                execStatus.className = 'playful-tag error';
            }

            let outputText = '';
            if (data.stdout) outputText += data.stdout;
            if (data.stderr) outputText += (outputText ? '\n' : '') + data.stderr;
            if (!outputText) outputText = '(Program finished cleanly with no output)';

            consoleOutput.textContent = outputText;
        } catch (err) {
            setRunningState(false);
            execStatus.textContent = '🔌 Disconnected';
            execStatus.className = 'playful-tag error';
            consoleOutput.textContent = `Could not reach the server:\n${err.message}`;
        }
    }

    function setRunningState(running) {
        if (running) {
            runBtn.disabled = true;
            runSpinner.classList.add('active');
            runIcon.style.display = 'none';
            statusDot.textContent = 'Cooking...';
            statusDot.className = 'pulsing-badge running';
        } else {
            runBtn.disabled = false;
            runSpinner.classList.remove('active');
            runIcon.style.display = 'inline-block';
            statusDot.textContent = 'Ready';
            statusDot.className = 'pulsing-badge';
        }
    }

    runBtn.addEventListener('click', runCode);

    // 4. Load Examples
    async function loadExamples() {
        try {
            const res = await fetch('/api/examples');
            const data = await res.json();
            exampleSelect.innerHTML = '<option value="">✨ Choose a fun template...</option>';

            if (data.examples && data.examples.length > 0) {
                data.examples.forEach((ex, idx) => {
                    const opt = document.createElement('option');
                    opt.value = idx;
                    // Look up a friendly playful name or fallback
                    let label = ex.name;
                    for (const [key, val] of Object.entries(friendlyNames)) {
                        if (ex.name.includes(key)) {
                            label = val;
                            break;
                        }
                    }
                    opt.textContent = label;
                    exampleSelect.appendChild(opt);
                });

                // Load first example
                editor.value = data.examples[0].code;
                activeFilename.textContent = data.examples[0].name.replace(' (test_sample.cpp)', '');
                updateLineNumbers();
            }
        } catch (err) {
            console.error('Failed to load examples:', err);
        }
    }

    exampleSelect.addEventListener('change', async (e) => {
        const idx = e.target.value;
        if (idx === '') return;
        try {
            const res = await fetch('/api/examples');
            const data = await res.json();
            const ex = data.examples[idx];
            if (ex) {
                editor.value = ex.code;
                let cleanName = ex.name.replace(' (test_sample.cpp)', '');
                for (const [key, val] of Object.entries(friendlyNames)) {
                    if (ex.name.includes(key)) {
                        cleanName = val;
                        break;
                    }
                }
                activeFilename.textContent = cleanName;
                updateLineNumbers();
                runCode();
            }
        } catch (err) {
            console.error(err);
        }
    });

    // 5. File Upload & Drag-and-Drop
    function handleFile(file) {
        if (!file) return;
        const reader = new FileReader();
        reader.onload = (e) => {
            editor.value = e.target.result;
            activeFilename.textContent = `📁 ${file.name}`;
            updateLineNumbers();
            runCode();
        };
        reader.readAsText(file);
    }

    fileUpload.addEventListener('change', (e) => {
        if (e.target.files.length > 0) {
            handleFile(e.target.files[0]);
        }
    });

    dropZone.addEventListener('dragover', (e) => {
        e.preventDefault();
        dropOverlay.classList.add('active');
    });

    dropZone.addEventListener('dragleave', (e) => {
        if (e.relatedTarget && !dropZone.contains(e.relatedTarget)) {
            dropOverlay.classList.remove('active');
        }
    });

    dropZone.addEventListener('drop', (e) => {
        e.preventDefault();
        dropOverlay.classList.remove('active');
        if (e.dataTransfer.files.length > 0) {
            handleFile(e.dataTransfer.files[0]);
        }
    });

    // 6. Clear Output
    clearBtn.addEventListener('click', () => {
        consoleOutput.textContent = 'Cleared! Hit "Run Code" when ready.';
        execStatus.textContent = '☕ Ready';
        execStatus.className = 'playful-tag';
    });

    // Initialize
    loadExamples();
});
