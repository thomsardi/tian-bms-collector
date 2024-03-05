// start set network //
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
  // Validation SSID
  const ssidInput = document.getElementById(elements.ssid);
  if (ssidInput.value.length < 8 || ssidInput.value.length > 16) {
    alert("Invalid SSID. SSID must be between 8 and 16 characters.");
    ssidInput.focus();
    return false;
  }

  // Validation Password
  const passwordInput = document.getElementById(elements.password);
  if (passwordInput.value.length < 8) {
    alert("Invalid Password. Password must be at least 8 characters long.");
    passwordInput.focus();
    return false;
  }

  // Validation IP Address, Gateway, and Subnet
  const ipKeys = ["ip", "gateway", "subnet"];
  for (const key of ipKeys) {
    let value = "";
    for (let i = 1; i <= 4; i++) {
      const ipInput = document.getElementById(`${elements[key]}${i}`);
      const ipValue = parseInt(ipInput.value);
      if (isNaN(ipValue) || ipValue < 0 || ipValue > 255) {
        alert(
          "Invalid IP Address. Each part of the IP address must be a number between 0 and 255."
        );
        ipInput.focus();
        return false;
      }
      value += ipValue + (i !== 4 ? "." : "");
    }
    result[key] = value;
  }

  // Data Collecting
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
  fetch("/api/set-network", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
    },
    body: JSON.stringify(result),
  })
    .then((response) => {
      if (!response.ok) {
        throw new Error("Network response was not ok");
      }
      return response.json();
    })
    .then((data) => {
      alert("Success");
    })
    .catch((error) => {
      console.error("Error:", error);
      alert("Error occurred. Please check the console for details.222");
    });
}
// end set network //

// start set modbus //
function onClickApplyModbusConfig() {
  const elements = {
    port: "port-input",
    ip: "modbus-ip",
  };
  const result = {};

  // Validation IP Address
  for (let i = 1; i <= 4; i++) {
    const ipInput = document.getElementById(`${elements.ip}${i}`);
    const ipValue = parseInt(ipInput.value);

    if (isNaN(ipValue) || ipValue < 0 || ipValue > 255) {
      alert(
        "Invalid Modbus IP. Each part of the Modbus IP must be a number between 0 and 255."
      );
      ipInput.focus();
      return false;
    }
  }

  // Validation Port
  const portInput = document.getElementById(elements.port);
  const portValue = parseInt(portInput.value);

  if (isNaN(portValue) || portValue < 0 || portValue > 65535) {
    alert(
      "Invalid Port. The port number must be a number between 0 and 65535."
    );
    portInput.focus();
    return false;
  }

  // Data Collecting
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
  fetch("/api/set-modbus", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
    },
    body: JSON.stringify(result),
  })
    .then((response) => {
      if (!response.ok) {
        throw new Error("Network response was not ok");
      }
      return response.json();
    })
    .then((data) => {
      alert("Success");
    })
    .catch((error) => {
      console.error("Error:", error);
      alert("Error occurred. Please check the console for details.222");
    });
}
// end set modbus //

// start set slave //
function onClickApplyModbusSlave() {
  const inputText = document.getElementById("slave-list-input").value.trim();
  let numbers = [];

  //validation Slave List
  const pattern = /\b(?:\d+(?:-\d+)?(?:,|$))+\b/;
  if (!pattern.test(inputText)) {
    alert(
      "Invalid Slave List. Slave List should be a number format separated by commas like 1,2,3,4 or like 1-4"
    );
    return false; // Tidak mengirim formulir jika validasi gagal
  }

  // Data Collecting
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
    slave_list: numbers,
  };

  console.log(result);
  fetch("/api/set-slave", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
    },
    body: JSON.stringify(result),
  })
    .then((response) => {
      if (!response.ok) {
        throw new Error("Network response was not ok");
      }
      return response.json();
    })
    .then((data) => {
      alert("Success");
    })
    .catch((error) => {
      console.error("Error:", error);
      alert("Error occurred. Please check the console for details.222");
    });
}
// end set slave //

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
