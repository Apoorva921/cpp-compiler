const http = require('http');
const fs = require('fs');
const path = require('path');
const { spawn, execFile } = require('child_process');
const os = require('os');
const crypto = require('crypto');

const PORT = process.env.PORT || 3000;
const ROOT_DIR = path.resolve(__dirname, '..');
const MCXX_BIN = path.join(ROOT_DIR, 'mcxx.exe');
const PUBLIC_DIR = path.join(__dirname, 'public');

// Ensure mcxx.exe exists
if (!fs.existsSync(MCXX_BIN)) {
    console.warn(`[WARNING] mcxx.exe not found at ${MCXX_BIN}. Please run 'mingw32-make' first.`);
}

function sendJson(res, statusCode, data) {
    const json = JSON.stringify(data);
    res.writeHead(statusCode, {
        'Content-Type': 'application/json',
        'Content-Length': Buffer.byteLength(json),
        'Access-Control-Allow-Origin': '*',
        'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
        'Access-Control-Allow-Headers': 'Content-Type'
    });
    res.end(json);
}

function sendFile(res, filePath, contentType) {
    fs.readFile(filePath, (err, data) => {
        if (err) {
            res.writeHead(404, { 'Content-Type': 'text/plain' });
            res.end('404 Not Found');
            return;
        }
        res.writeHead(200, {
            'Content-Type': contentType,
            'Content-Length': data.length,
            'Cache-Control': 'no-cache'
        });
        res.end(data);
    });
}

function parseBody(req) {
    return new Promise((resolve, reject) => {
        let body = '';
        req.on('data', chunk => {
            body += chunk.toString();
            if (body.length > 5 * 1024 * 1024) { // 5MB limit
                req.destroy();
                reject(new Error('Payload too large'));
            }
        });
        req.on('end', () => {
            try {
                if (req.headers['content-type'] && req.headers['content-type'].includes('application/json')) {
                    resolve(JSON.parse(body || '{}'));
                } else {
                    resolve({ code: body });
                }
            } catch (err) {
                resolve({ code: body });
            }
        });
        req.on('error', reject);
    });
}

// Sandbox execution helper
function runMcxxCommand(args, code, stdinInput = "10\n20\n", timeoutMs = 8000) {
    return new Promise((resolve) => {
        const id = crypto.randomBytes(8).toString('hex');
        const tempDir = path.join(os.tmpdir(), `mcxx_${id}`);
        fs.mkdirSync(tempDir, { recursive: true });

        const inputPath = path.join(tempDir, 'input.cpp');
        fs.writeFileSync(inputPath, code, 'utf8');

        // Replace placeholder paths in args
        const resolvedArgs = args.map(a => {
            if (a === '<INPUT>') return inputPath;
            if (a === '<OUTPUT_EXE>') return path.join(tempDir, 'out.exe');
            if (a === '<OUTPUT_S>') return path.join(tempDir, 'out.s');
            return a;
        });

        const startTime = Date.now();
        let stdout = '';
        let stderr = '';
        let timedOut = false;

        const child = spawn(MCXX_BIN, resolvedArgs, {
            cwd: tempDir,
            windowsHide: true
        });

        if (child.stdin) {
            child.stdin.write(stdinInput || "10\n20\n");
            child.stdin.end();
        }

        const timer = setTimeout(() => {
            timedOut = true;
            try { child.kill('SIGKILL'); } catch (e) {}
        }, timeoutMs);

        child.stdout.on('data', d => { stdout += d.toString(); });
        child.stderr.on('data', d => { stderr += d.toString(); });

        child.on('close', (code) => {
            clearTimeout(timer);
            const timeMs = Date.now() - startTime;

            let extraFiles = {};
            const asmPath = path.join(tempDir, 'out.s');
            if (fs.existsSync(asmPath)) {
                try {
                    extraFiles.assembly = fs.readFileSync(asmPath, 'utf8');
                } catch (e) {}
            }

            // Cleanup temp directory
            try {
                fs.rmSync(tempDir, { recursive: true, force: true });
            } catch (e) {}

            resolve({
                success: code === 0 && !timedOut,
                exitCode: timedOut ? -1 : code,
                timedOut,
                stdout,
                stderr,
                timeMs,
                ...extraFiles
            });
        });

        child.on('error', (err) => {
            clearTimeout(timer);
            try { fs.rmSync(tempDir, { recursive: true, force: true }); } catch (e) {}
            resolve({
                success: false,
                exitCode: -1,
                timedOut: false,
                stdout: '',
                stderr: `Failed to execute compiler: ${err.message}`,
                timeMs: Date.now() - startTime
            });
        });
    });
}

const server = http.createServer(async (req, res) => {
    // CORS preflight
    if (req.method === 'OPTIONS') {
        res.writeHead(204, {
            'Access-Control-Allow-Origin': '*',
            'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
            'Access-Control-Allow-Headers': 'Content-Type'
        });
        res.end();
        return;
    }

    const parsedUrl = new URL(req.url, `http://${req.headers.host}`);
    const pathname = parsedUrl.pathname;

    // Static Assets
    if (req.method === 'GET') {
        if (pathname === '/' || pathname === '/index.html') {
            return sendFile(res, path.join(PUBLIC_DIR, 'index.html'), 'text/html');
        }
        if (pathname === '/style.css') {
            return sendFile(res, path.join(PUBLIC_DIR, 'style.css'), 'text/css');
        }
        if (pathname === '/app.js') {
            return sendFile(res, path.join(PUBLIC_DIR, 'app.js'), 'application/javascript');
        }

        // GET /api/examples: return list of ready-to-run programs
        if (pathname === '/api/examples') {
            const examples = [];
            const exDir = path.join(ROOT_DIR, 'examples');
            if (fs.existsSync(exDir)) {
                const files = fs.readdirSync(exDir).filter(f => f.endsWith('.cpp'));
                for (const f of files) {
                    const content = fs.readFileSync(path.join(exDir, f), 'utf8');
                    examples.push({ name: f, code: content });
                }
            }
            const samplePath = path.join(ROOT_DIR, 'test_sample.cpp');
            if (fs.existsSync(samplePath)) {
                examples.unshift({ name: 'Calculator OOP (test_sample.cpp)', code: fs.readFileSync(samplePath, 'utf8') });
            }
            return sendJson(res, 200, { examples });
        }
    }

    // POST APIs
    if (req.method === 'POST') {
        try {
            const body = await parseBody(req);
            const code = body.code || '';

            if (!code.trim()) {
                return sendJson(res, 400, { success: false, error: 'No C++ code provided' });
            }

            // POST /api/run: compile, link, and run
            if (pathname === '/api/run') {
                const result = await runMcxxCommand(['-run', '<INPUT>', '-o', '<OUTPUT_EXE>'], code);
                return sendJson(res, 200, result);
            }

            // POST /api/assembly: emit x86 assembly (.s)
            if (pathname === '/api/assembly') {
                const result = await runMcxxCommand(['-S', '<INPUT>', '-o', '<OUTPUT_S>'], code);
                return sendJson(res, 200, result);
            }

            // POST /api/ast: dump AST
            if (pathname === '/api/ast') {
                const result = await runMcxxCommand(['--dump-ast', '<INPUT>'], code);
                return sendJson(res, 200, result);
            }

            // POST /api/tokens: dump tokens
            if (pathname === '/api/tokens') {
                const result = await runMcxxCommand(['--dump-tokens', '<INPUT>'], code);
                return sendJson(res, 200, result);
            }
        } catch (err) {
            return sendJson(res, 500, { success: false, error: err.message });
        }
    }

    res.writeHead(404, { 'Content-Type': 'text/plain' });
    res.end('404 Not Found');
});

server.listen(PORT, '0.0.0.0', () => {
    console.log(`====================================================`);
    console.log(` Micro C++ (mcxx) Online Playground & REST API Server`);
    console.log(` Listening on: http://localhost:${PORT}`);
    console.log(` Network access: http://0.0.0.0:${PORT}`);
    console.log(` API Endpoint: POST http://localhost:${PORT}/api/run`);
    console.log(`====================================================`);
});
