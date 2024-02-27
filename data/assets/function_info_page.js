// start set network
function onClickApply() {
  const elements = {
    mode: "mode",
    server: "server",
    ssid: "ssid",
    password: "password",
    ip: "device-ip",
    gateway: "gateway-ip",
    subnet: "subnet-ip",
  };
  const result = {};
  for (const key in elements) {
    if (Object.hasOwnProperty.call(elements, key)) {
      if (key === "mode" || key === "server") {
        result[key] = parseInt(document.getElementById(elements[key]).value);
      } else if (key === "ip" || key === "gateway" || key === "subnet") {
        let value = "";
        for (let i = 1; i <= 4; i++) {
          const inputValue = parseInt(
            document.getElementById(`${elements[key]}${i}`).value
          );
          value += inputValue + (i !== 4 ? "." : "");
        }
        result[key] = value;
      } else {
        result[key] = document.getElementById(elements[key]).value;
      }
    }
  }
  console.log(result);
  const xhr = new XMLHttpRequest();
  xhr.open("POST", "/api/set-network");
  xhr.setRequestHeader("Content-Type", "application/json");
  xhr.onload = function () {
    if (xhr.status === 200) {
      alert("Success");
      window.location.href = "/";
    } else {
      console.error("Error:", xhr.status);
    }
  };
  xhr.send(JSON.stringify(result));
  return true;
}
// end set network

// start set modbus
function onClickApplyModbusConfig() {
  const elements = {
    port: "port-input",
    ip: "modbus-ip",
  };
  const result = {};
  for (const key in elements) {
    if (Object.hasOwnProperty.call(elements, key)) {
      if (key === "port") {
        result[key] = parseInt(document.getElementById(elements[key]).value);
      } else {
        let value = "";
        for (let i = 1; i <= 4; i++) {
          const inputValue = parseInt(
            document.getElementById(`${elements[key]}${i}`).value
          );
          value += inputValue + (i !== 4 ? "." : "");
        }
        result[key] = value;
      }
    }
  }
  console.log(result);
  const xhr = new XMLHttpRequest();
  xhr.open("POST", "/api/set-modbus");
  xhr.setRequestHeader("Content-Type", "application/json");
  xhr.onload = function () {
    if (xhr.status === 200) {
      alert("Success");
      window.location.href = "/";
    } else {
      console.error("Error:", xhr.status);
    }
  };
  xhr.send(JSON.stringify(result));
  return true;
}
// end set modbus

// start set slave
function onClickApplyModbusSlave() {
  const inputText = document.getElementById("slave-list-input").value.trim();
  let numbers = [];

  // Check if the input contains a range (e.g., "1-4")
  if (inputText.includes("-")) {
    const ranges = inputText.split(",");
    ranges.forEach((range) => {
      if (range.includes("-")) {
        const [start, end] = range.split("-");
        const startNum = parseInt(start);
        const endNum = parseInt(end);

        if (!isNaN(startNum) && !isNaN(endNum)) {
          for (let i = startNum; i <= endNum; i++) {
            if (!numbers.includes(i)) {
              numbers.push(i);
            }
          }
        }
      } else {
        const singleNum = parseInt(range.trim());
        if (!isNaN(singleNum) && !numbers.includes(singleNum)) {
          numbers.push(singleNum);
        }
      }
    });
  } else {
    // Check if the input contains individual numbers separated by commas
    const nums = inputText.split(",");
    nums.forEach((num) => {
      const parsedNum = parseInt(num.trim());
      if (!isNaN(parsedNum) && !numbers.includes(parsedNum)) {
        numbers.push(parsedNum);
      }
    });
  }
  const result = {
    slave_set: 1,
    slave_list: numbers,
  };

  console.log(result);
  const xhr = new XMLHttpRequest();
  xhr.open("POST", "/api/set-slave");
  xhr.setRequestHeader("Content-Type", "application/json");
  xhr.onload = function () {
    if (xhr.status === 200) {
      alert("Success");
      window.location.href = "/";
    } else {
      console.error("Error:", xhr.status);
    }
  };
  xhr.send(JSON.stringify(result));
  return true;
}
// end set slave

window.onload = function () {
  fetchData("/api/get-device-info", changeDeviceInfoContent);
  fetchData("/api/get-modbus-info", changeDeviceInfoModbusContent);
};

function fetchData(url, callback) {
  fetch(url)
    .then((response) => {
      if (!response.ok) {
        throw new Error("Network response was not ok");
      }
      clearTimeout();
      return response.json();
    })
    .then((data) => {
      // Handle the response data
      callback(data);
      setTimeout(fetchData, 500, url, callback);
      // console.log(data);
    })
    .catch((error) => {
      // Handle errors
      setTimeout(fetchData, 1000, url, callback);
      console.error("Fetch error:", error);
    });
}

function changeDeviceInfoContent(data) {
  if ("firmware_version" in data) {
    const spanElement = document.getElementById("firmware-version");
    spanElement.textContent = data["firmware_version"];
  }
  if ("mode" in data) {
    const spanElement = document.getElementById("device-mode");
    spanElement.textContent = data["mode"];
  }
  if ("device_ip" in data) {
    const spanElement = document.getElementById("device-ip");
    spanElement.textContent = data["device_ip"];
  }
  if ("mac_address" in data) {
    const spanElement = document.getElementById("mac-address");
    spanElement.textContent = data["mac_address"];
  }
  if ("server_mode" in data) {
    const spanElement = document.getElementById("server-mode");
    spanElement.textContent = data["server_mode"];
  }
  if ("ssid" in data) {
    const spanElement = document.getElementById("device-ssid");
    spanElement.textContent = data["ssid"];
  }
}

function changeDeviceInfoModbusContent(data) {
  if ("modbus_ip" in data) {
    const spanElement = document.getElementById("modbus-ip");
    spanElement.textContent = data["modbus_ip"];
  }
  if ("port" in data) {
    const spanElement = document.getElementById("port");
    spanElement.textContent = data["port"];
  }
  if ("slave_list" in data) {
    const spanElement = document.getElementById("slave-list");
    spanElement.textContent = data["slave_list"];
  }
}

function onClickReboot() {
  const xhr = new XMLHttpRequest();
  xhr.open("POST", "/api/restart");
  xhr.setRequestHeader("Content-Type", "application/json");
  xhr.onload = function () {
    if (xhr.status === 200) {
      alert("Success");
    } else {
      console.error("Error:", xhr.status);
    }
  };
  xhr.send(JSON.stringify({ restart: 1 }));
}

function onClickFactoryReset() {
  const xhr = new XMLHttpRequest();
  xhr.open("POST", "/api/freset");
  xhr.setRequestHeader("Content-Type", "application/json");
  xhr.onload = function () {
    if (xhr.status === 200) {
      alert("Success");
    } else {
      console.error("Error:", xhr.status);
    }
  };
  xhr.send(JSON.stringify({ freset: 1 }));
}
