import { createApp, ref, reactive, computed } from "vue/dist/vue.esm-bundler.js";
import Antd, { message } from "ant-design-vue";
import "ant-design-vue/dist/reset.css";

const App = {
  setup() {
    const baseUrl = ref("http://127.0.0.1:35182");
    const authToken = ref("");
    const method = ref("GET");
    const path = ref("/status");
    const listPath = ref("C:\\\\");
    const searchPath = ref("C:\\\\");
    const searchQuery = ref("");
    const searchCase = ref("i");
    const searchMax = ref(50);
    const readPath = ref("C:\\\\");
    const readStart = ref("");
    const readLines = ref("");
    const readTail = ref("");
    const readCount = ref(false);
    const clipboardContent = ref("");
    const screenshotFormat = ref("png");
    const requestBody = ref("");
    const loading = ref(false);
    const lastDurationMs = ref(null);

    const history = ref([]);
    const response = reactive({
      ok: false,
      status: "",
      headers: [],
      body: "",
      error: ""
    });

    const fullUrl = computed(() => {
      const base = baseUrl.value.replace(/\/$/, "");
      const p = path.value.startsWith("/") ? path.value : `/${path.value}`;
      return `${base}${p}`;
    });

    const presetEndpoints = [
      { label: "GET /", method: "GET", path: "/" },
      { label: "GET /status", method: "GET", path: "/status" },
      { label: "GET /sts", method: "GET", path: "/sts" },
      { label: "GET /health", method: "GET", path: "/health" },
      { label: "GET /disks", method: "GET", path: "/disks" },
      { label: "GET /clipboard", method: "GET", path: "/clipboard" },
      { label: "GET /screenshot?format=png", method: "GET", path: "/screenshot?format=png" },
      { label: "POST /status", method: "POST", path: "/status" },
      { label: "POST /exit", method: "POST", path: "/exit" }
    ];

    function setPreset(preset) {
      method.value = preset.method;
      path.value = preset.path;
    }

    async function sendRequest() {
      loading.value = true;
      response.ok = false;
      response.status = "";
      response.headers = [];
      response.body = "";
      response.error = "";
      lastDurationMs.value = null;

      const start = performance.now();
      try {
        const options = {
          method: method.value,
          headers: {
            "Content-Type": "application/json"
          }
        };
        if (authToken.value.trim().length > 0) {
          options.headers["Authorization"] = `Bearer ${authToken.value.trim()}`;
        }

        if (method.value !== "GET" && requestBody.value.trim().length > 0) {
          options.body = requestBody.value;
        }

        const res = await fetch(fullUrl.value, options);
        const text = await res.text();

        let prettyBody = text;
        try {
          const parsed = JSON.parse(text);
          prettyBody = JSON.stringify(parsed, null, 2);
        } catch (_) {
          // keep raw text
        }

        response.ok = res.ok;
        response.status = `${res.status} ${res.statusText}`;
        response.headers = Array.from(res.headers.entries());
        response.body = prettyBody;

        history.value.unshift({
          time: new Date().toLocaleTimeString(),
          method: method.value,
          url: fullUrl.value,
          status: response.status
        });
      } catch (err) {
        response.error = err?.message || String(err);
        message.error(`请求失败: ${response.error}`);
      } finally {
        lastDurationMs.value = Math.round(performance.now() - start);
        loading.value = false;
      }
    }

    function clearHistory() {
      history.value = [];
    }

    function useListPath() {
      path.value = `/list?path=${encodeURIComponent(listPath.value)}`;
      method.value = "GET";
    }

    function useSearch() {
      const params = new URLSearchParams();
      params.set("path", searchPath.value);
      if (searchQuery.value) params.set("query", searchQuery.value);
      if (searchCase.value) params.set("case", searchCase.value);
      if (searchMax.value) params.set("max", String(searchMax.value));
      path.value = `/search?${params.toString()}`;
      method.value = "GET";
    }

    function useRead() {
      const params = new URLSearchParams();
      params.set("path", readPath.value);
      if (readStart.value !== "") params.set("start", String(readStart.value));
      if (readLines.value !== "") params.set("lines", String(readLines.value));
      if (readTail.value !== "") params.set("tail", String(readTail.value));
      if (readCount.value) params.set("count", "true");
      path.value = `/read?${params.toString()}`;
      method.value = "GET";
    }

    function useClipboardGet() {
      method.value = "GET";
      path.value = "/clipboard";
      requestBody.value = "";
    }

    function useClipboardPut() {
      method.value = "PUT";
      path.value = "/clipboard";
      requestBody.value = JSON.stringify({ content: clipboardContent.value || "" });
    }

    function useScreenshot() {
      method.value = "GET";
      path.value = `/screenshot?format=${encodeURIComponent(screenshotFormat.value)}`;
      requestBody.value = "";
    }

    function setDisks() {
      method.value = "GET";
      path.value = "/disks";
      requestBody.value = "";
    }

    function setExit() {
      method.value = "GET";
      path.value = "/exit";
      requestBody.value = "";
    }

    async function quickDisks() {
      setDisks();
      await sendRequest();
    }

    async function quickList() {
      useListPath();
      await sendRequest();
    }

    async function quickSearch() {
      useSearch();
      await sendRequest();
    }

    async function quickRead() {
      useRead();
      await sendRequest();
    }

    async function quickClipboardGet() {
      useClipboardGet();
      await sendRequest();
    }

    async function quickClipboardPut() {
      useClipboardPut();
      await sendRequest();
    }

    async function quickScreenshot() {
      useScreenshot();
      await sendRequest();
    }

    async function quickExit() {
      setExit();
      await sendRequest();
    }

    return {
      baseUrl,
      authToken,
      method,
      path,
      listPath,
      searchPath,
      searchQuery,
      searchCase,
      searchMax,
      readPath,
      readStart,
      readLines,
      readTail,
      readCount,
      clipboardContent,
      screenshotFormat,
      requestBody,
      response,
      history,
      loading,
      lastDurationMs,
      presetEndpoints,
      setPreset,
      useListPath,
      useSearch,
      useRead,
      useClipboardGet,
      useClipboardPut,
      useScreenshot,
      setDisks,
      setExit,
      quickDisks,
      quickList,
      quickSearch,
      quickRead,
      quickClipboardGet,
      quickClipboardPut,
      quickScreenshot,
      quickExit,
      sendRequest,
      clearHistory,
      fullUrl
    };
  },
  template: `
    <a-layout style="min-height: 100vh; background: #f5f7fb;">
      <a-layout-header style="background: #0b1b2b; display: flex; align-items: center;">
        <div style="color: #fff; font-size: 18px; font-weight: 600;">ClawDesk MCP Test Client</div>
      </a-layout-header>
      <a-layout-content style="padding: 24px;">
        <a-row :gutter="16">
          <a-col :span="16">
            <a-card title="快捷操作" :bordered="false">
              <a-space direction="vertical" style="width: 100%;">
                <a-space>
                  <a-button type="primary" :loading="loading" @click="quickDisks">获取磁盘</a-button>
                  <a-button :loading="loading" @click="quickList">列目录</a-button>
                  <a-button :loading="loading" @click="quickSearch">搜索文本</a-button>
                  <a-button :loading="loading" @click="quickRead">读取文件</a-button>
                  <a-button :loading="loading" @click="quickClipboardGet">读取剪贴板</a-button>
                  <a-button :loading="loading" @click="quickClipboardPut">写入剪贴板</a-button>
                  <a-button :loading="loading" @click="quickScreenshot">截图</a-button>
                  <a-button danger :loading="loading" @click="quickExit">退出服务</a-button>
                </a-space>
                <a-space style="width: 100%;" align="start">
                  <a-input v-model:value="listPath" addon-before="List 路径" placeholder="C:\\" />
                  <a-button @click="useListPath">生成 /list 请求</a-button>
                </a-space>
                <a-space style="width: 100%;" align="start">
                  <a-input v-model:value="searchPath" addon-before="Search 路径" placeholder="C:\\" />
                  <a-input v-model:value="searchQuery" addon-before="关键字" placeholder="text" />
                  <a-select v-model:value="searchCase" style="width: 120px;">
                    <a-select-option value="i">忽略大小写</a-select-option>
                    <a-select-option value="sensitive">区分大小写</a-select-option>
                  </a-select>
                  <a-input-number v-model:value="searchMax" :min="1" :max="1000" addon-before="Max" />
                  <a-button @click="useSearch">生成 /search 请求</a-button>
                </a-space>
                <a-space style="width: 100%;" align="start">
                  <a-input v-model:value="readPath" addon-before="Read 路径" placeholder="C:\\" />
                  <a-input v-model:value="readStart" addon-before="start" placeholder="1" style="width: 140px;" />
                  <a-input v-model:value="readLines" addon-before="lines" placeholder="200" style="width: 150px;" />
                  <a-input v-model:value="readTail" addon-before="tail" placeholder="200" style="width: 140px;" />
                  <a-switch v-model:checked="readCount" checked-children="count" un-checked-children="count" />
                  <a-button @click="useRead">生成 /read 请求</a-button>
                </a-space>
                <a-space style="width: 100%;" align="start">
                  <a-input v-model:value="clipboardContent" addon-before="Clipboard" placeholder="要写入的文本" />
                  <a-button @click="useClipboardPut">生成 /clipboard PUT</a-button>
                  <a-button @click="useClipboardGet">生成 /clipboard GET</a-button>
                </a-space>
                <a-space style="width: 100%;" align="start">
                  <a-select v-model:value="screenshotFormat" style="width: 140px;">
                    <a-select-option value="png">png</a-select-option>
                    <a-select-option value="jpg">jpg</a-select-option>
                  </a-select>
                  <a-button @click="useScreenshot">生成 /screenshot 请求</a-button>
                </a-space>
                <div style="color: #6b7280;">
                  快捷按钮会自动填充请求并立即发送；可先填写路径再点“列目录”。
                </div>
              </a-space>
            </a-card>

            <a-card title="自定义请求" style="margin-top: 16px;" :bordered="false">
              <a-space direction="vertical" style="width: 100%;">
                <a-input v-model:value="baseUrl" addon-before="Base URL" placeholder="http://127.0.0.1:35182" />
                <a-input v-model:value="authToken" addon-before="Auth Token" placeholder="Bearer token" />
                <a-space style="width: 100%;" align="start">
                  <a-select v-model:value="method" style="width: 120px;">
                    <a-select-option value="GET">GET</a-select-option>
                    <a-select-option value="POST">POST</a-select-option>
                  </a-select>
                  <a-input v-model:value="path" style="flex: 1;" placeholder="/status" />
                  <a-button type="primary" :loading="loading" @click="sendRequest">发送</a-button>
                </a-space>
                <a-textarea v-model:value="requestBody" :rows="4" placeholder="可选 JSON 请求体" />
                <a-space wrap>
                  <a-button v-for="preset in presetEndpoints" :key="preset.label" @click="setPreset(preset)">
                    {{ preset.label }}
                  </a-button>
                </a-space>
                <div style="color: #6b7280;">
                  实际请求: <code>{{ fullUrl }}</code>
                </div>
              </a-space>
            </a-card>

            <a-card title="响应" style="margin-top: 16px;" :bordered="false">
              <a-space direction="vertical" style="width: 100%;">
                <div v-if="response.error" style="color: #b91c1c;">{{ response.error }}</div>
                <div v-else>
                  <div><strong>状态:</strong> {{ response.status || '-' }}</div>
                  <div><strong>耗时:</strong> {{ lastDurationMs !== null ? lastDurationMs + ' ms' : '-' }}</div>
                </div>
                <a-divider />
                <div><strong>Headers</strong></div>
                <a-table
                  :columns="[
                    { title: 'Key', dataIndex: 'key', key: 'key' },
                    { title: 'Value', dataIndex: 'value', key: 'value' }
                  ]"
                  :data-source="response.headers.map(([key, value]) => ({ key, value }))"
                  size="small"
                  :pagination="false"
                />
                <a-divider />
                <div><strong>Body</strong></div>
                <a-textarea :value="response.body" :rows="10" readonly />
              </a-space>
            </a-card>
          </a-col>

          <a-col :span="8">
            <a-card title="历史" :bordered="false">
              <a-space direction="vertical" style="width: 100%;">
                <a-button danger @click="clearHistory">清空历史</a-button>
                <a-list :data-source="history">
                  <template #renderItem="{ item }">
                    <a-list-item>
                      <a-list-item-meta
                        :title="item.method + ' ' + item.url"
                        :description="item.time + ' | ' + item.status"
                      />
                    </a-list-item>
                  </template>
                </a-list>
              </a-space>
            </a-card>

            <a-card title="说明" style="margin-top: 16px;" :bordered="false">
              <div style="color: #6b7280; line-height: 1.6;">
                当前可用测试端点: <code>/</code>、<code>/health</code>、<code>/status</code>、<code>/sts</code>、<code>/disks</code>、<code>/list</code>、
                <code>/search</code>、<code>/read</code>、<code>/clipboard</code>、<code>/screenshot</code>、<code>/exit</code>。
                上方表单会自动拼接参数并填充到“自定义请求”区域。
              </div>
            </a-card>
          </a-col>
        </a-row>
      </a-layout-content>
    </a-layout>
  `
};

const app = createApp(App);
app.use(Antd);
app.mount("#app");
