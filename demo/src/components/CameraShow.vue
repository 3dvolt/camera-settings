<script lang="ts" setup>
import { onMounted } from "vue";
import { ref } from "vue";
import { cameraId, resolutions } from "../store";
import { watch } from "vue";
import { computed } from "vue";

const videoRef = ref<HTMLVideoElement>();

const cameraList = ref<MediaDeviceInfo[]>([]);

async function initCamera(re = false) {
  try {
    const devices = await navigator.mediaDevices.enumerateDevices();
    console.log(devices)
    const cameras = devices.filter((device) => device.kind === "videoinput");
    cameraList.value = cameras;
  } catch (error) {
    if (!re) {
      // If no permission, first attempt will fail, request permission and retry
      const stream = await navigator.mediaDevices.getUserMedia({ video: true });
      stream.getTracks().forEach((track) => track.stop());
      initCamera();
      return;
    }
    console.error(error);
  }
}

onMounted(() => {
  initCamera();
});

let stream: MediaStream | null = null;
const isOpen = ref(false);

async function openCapture() {
  if (!videoRef.value) {
    return;
  }
  if (stream) {
    closeCapture();
  }

  const resolution = selectedResolution.value.split("x").map((s) => +s);

  const camera = cameraList.value[cameraId.value];
  stream = await navigator.mediaDevices.getUserMedia({
    video: {
      deviceId: camera.deviceId,
      width: resolution[0],
      height: resolution[1],
    },
  });
  isOpen.value = true;
  videoRef.value.srcObject = stream;
}

function closeCapture() {
  stream?.getTracks().forEach((track) => track.stop());
  stream = null;
  isOpen.value = false;
}

const resolutionsEx = computed(() => {
  const rr = resolutions.value.map((r) => `${r.width}x${r.height}`);
  return Array.from(new Set(rr));
});

const selectedResolution = ref("640x480");
watch(selectedResolution, () => {
  if (isOpen.value) {
    openCapture();
  }
});
</script>

<template>
  <div class="camera-show">
    <h2>Camera</h2>
    <video ref="videoRef" width="640" height="480" autoplay></video>
    <div class="ctrl-row">
      <div class="ctrl-left">
        <ElSelect
          v-model="cameraId"
          placeholder="Select camera"
          :disabled="isOpen"
        >
          <ElOption
            v-for="(camera, index) in cameraList"
            :key="camera.deviceId"
            :label="camera.label"
            :value="index"
          ></ElOption>
        </ElSelect>
      </div>
      <div class="ctrl-left">
        <ElSelect v-model="selectedResolution" placeholder="Select resolution">
          <ElOption
            v-for="resolution in resolutionsEx"
            :key="resolution"
            :label="resolution"
            :value="resolution"
          ></ElOption>
        </ElSelect>
      </div>
      <div class="ctrl-right">
        <ElButton type="primary" :disabled="isOpen" @click="openCapture"
          >Open</ElButton
        >
        <ElButton type="danger" :disabled="!isOpen" @click="closeCapture"
          >Close</ElButton
        >
      </div>
    </div>
  </div>
</template>

<style lang="scss" scoped>
.camera-show {
  width: 640px;
  video {
    border: 1px solid #ccc;
  }
  .ctrl-row {
    display: flex;
    justify-content: space-between;
  }
}
</style>
