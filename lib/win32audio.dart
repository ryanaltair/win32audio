// ignore_for_file: public_member_api_docs, sort_constructors_first

import "package:flutter/services.dart";

class AudioDevice {
  String id = "";
  String name = "";
  String iconPath = "";
  int iconID = 0;
  bool isActive = false;
  @override
  String toString() {
    return "AudioDevice{id: $id, name: $name, iconPath: $iconPath, iconID: $iconID, isActive: $isActive}";
  }

  //tomap
  Map<String, dynamic> toMap() {
    return <String, dynamic>{
      "id": id,
      "name": name,
      "iconPath": iconPath,
      "iconID": iconID,
      "isActive": isActive,
    };
  }

  @override
  bool operator ==(Object other) {
    if (identical(this, other)) return true;

    return other is AudioDevice && other.id == id && other.name == name && other.iconPath == iconPath && other.iconID == iconID && other.isActive == isActive;
  }

  @override
  int get hashCode {
    return id.hashCode ^ name.hashCode ^ iconPath.hashCode ^ iconID.hashCode ^ isActive.hashCode;
  }
}
 
enum AudioDeviceType {
  output,
  input,
}

enum AudioRole {
  console,
  multimedia,
  communications,
}

const MethodChannel audioMethodChannel = MethodChannel("win32audio");

class Audio {
  static bool listenerActive = false;

  static final List<void Function(String type, String id)> _changeListeners = <void Function(String type, String id)>[];

  static Future<void> setupChangeListener() async {
    if (!listenerActive) {
      listenerActive = true;
      await audioMethodChannel.invokeMethod('initAudioListener');
      audioMethodChannel.setMethodCallHandler((MethodCall call) async {
        if (call.method == 'onAudioDeviceChange') {
          for (void Function(String type, String id) listener in _changeListeners) {
            listener(call.arguments["name"], call.arguments["id"]);
          }
        }
      });
    }
  }

  static void addChangeListener(void Function(String type, String id) callback) {
    _changeListeners.add(callback);
  }

  static void removeChangeListener(void Function(String type, String id) callback) {
    _changeListeners.remove(callback);
  }

  /// Returns a Future list of audio devices of a specified type.
  /// The type is specified by the [AudioDeviceType] enum.
  ///
  static Future<List<AudioDevice>?> enumDevices(AudioDeviceType audioDeviceType, {AudioRole audioRole = AudioRole.multimedia}) async {
    final Map<String, dynamic> arguments = <String, dynamic>{"deviceType": audioDeviceType.index, "role": audioRole.index};
    final Map<dynamic, dynamic> map = await audioMethodChannel.invokeMethod("enumAudioDevices", arguments);
    List<AudioDevice>? audioDevices = <AudioDevice>[];
    for (int key in map.keys) {
      final AudioDevice audioDevice = AudioDevice();
      audioDevice.id = map[key]["id"];
      audioDevice.name = map[key]["name"];
      final List<String> iconData = map[key]["iconInfo"].split(",");
      if (iconData.length == 2) {
        audioDevice.iconPath = iconData[0];
        audioDevice.iconID = int.tryParse(iconData[1]) ?? -1;
      } else {
        audioDevice.iconPath = map[key]["iconInfo"];
        audioDevice.iconID = -1;
      }
      audioDevice.isActive = map[key]["isActive"];
      audioDevices.add(audioDevice);
    }
    return audioDevices;
  }
 
  
}
 
 