# Acoustic Wave Test
This GUI tests reception of `pcm` data returned by Xiaozhi devices over `udp` and converts it to time/frequency-domain views. It can save audio up to the window length to inspect noise-frequency distribution and test the accuracy of acoustic-wave ASCII transmission.

For firmware testing, enable `USE_AUDIO_DEBUGGER`,  and set `AUDIO_DEBUG_UDP_SERVER` to this machine’s address.
Acoustic-wave `demod` demodulation can be tested with `sonic_wifi_config.html` or by uploading it to `PinMe` [Xiaozhi acoustic-wave provisioning](https://iqf7jnhi.pinit.eth.limo) to run the acoustic-wave test.

# Acoustic-Wave Decoding Test Results

> `✓`means decoding succeeds directly from raw PCM received on I2S DIN,  `△`means noise reduction or other processing is needed for stable decoding, and  `X`means results remain poor after noise reduction (partial decoding may work, but is highly unstable).
> Some ADCs require finer noise-reduction tuning during I2C configuration. Since devices differ, testing currently uses only the configurations provided under boards.

| Device | ADC | MIC | Result | Notes |
| ---- | ---- | --- | --- | ---- |
| bread-compact | INMP441 | Integrated MEMEMIC | ✓ |
| atk-dnesp32s3-box | ES8311 | | ✓ |
| magiclick-2p5 | ES8311 | | ✓ |
| lichuang-dev  | ES7210 | | △ | Disable INPUT_REFERENCE
| kevin-box-2 | ES7210 | | △ | Disable INPUT_REFERENCE
| m5stack-core-s3 | ES7210 | | △ | Disable INPUT_REFERENCE
| xmini-c3 | ES8311 | | △ | Noise reduction required
| atoms3r-echo-base | ES8311 | | △ | Noise reduction required
| atk-dnesp32s3-box0 | ES8311 | | X | Reception and decoding work, but the packet-loss rate is very high
| movecall-moji-esp32s3 | ES8311 | | X | Reception and decoding work, but the packet-loss rate is very high