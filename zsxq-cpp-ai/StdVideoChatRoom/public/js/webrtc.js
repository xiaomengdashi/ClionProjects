/**
 * WebRTC管理类
 * 负责处理所有WebRTC相关的连接和媒体流管理
 */
class WebRTCManager {
    constructor() {
        this.localStream = null;
        this.peerConnections = new Map();
        this.isMobile = this.detectMobile();
        this.configuration = {
            iceServers: [
                { urls: 'stun:stun.l.google.com:19302' },
                { urls: 'stun:stun1.l.google.com:19302' }
            ]
        };
    }

    /**
     * 检测是否为移动设备
     * @returns {boolean}
     */
    detectMobile() {
        return /Android|webOS|iPhone|iPad|iPod|BlackBerry|IEMobile|Opera Mini/i.test(navigator.userAgent);
    }

    /**
     * 获取适合设备的媒体约束 - 统一16:9比例确保一致性
     * @returns {Object}
     */
    getMediaConstraints() {
        // 统一使用16:9比例确保所有设备视频画面一致
        const baseConstraints = {
            audio: {
                echoCancellation: true,
                noiseSuppression: true,
                autoGainControl: true
            }
        };
        
        if (this.isMobile) {
            return {
                ...baseConstraints,
                video: {
                    width: { ideal: 854, max: 854 },    // 16:9比例
                    height: { ideal: 480, max: 480 },   // 16:9比例
                    frameRate: { ideal: 15, max: 24 },
                    facingMode: 'user',
                    aspectRatio: 16/9  // 强制16:9比例
                }
            };
        } else {
            return {
                ...baseConstraints,
                video: {
                    width: { ideal: 1280, max: 1920 },
                    height: { ideal: 720, max: 1080 },
                    frameRate: { ideal: 30, max: 30 },
                    facingMode: 'user',
                    aspectRatio: 16/9  // 强制16:9比例
                }
            };
        }
    }

    /**
     * 初始化本地媒体流
     * @returns {Promise<MediaStream>}
     */
    async initLocalStream() {
        try {
            const constraints = this.getMediaConstraints();
            this.localStream = await navigator.mediaDevices.getUserMedia(constraints);
            
            const localVideo = document.getElementById('localVideo');
            localVideo.srcObject = this.localStream;
            
            // 验证视频流质量和比例
            this.validateVideoStream(this.localStream);
            
            // 移动端强制播放
            if (this.isMobile) {
                try {
                    await localVideo.play();
                } catch (e) {
                    console.log('本地视频自动播放失败，这是正常的');
                }
            }
            
            return this.localStream;
        } catch (error) {
            console.error('获取媒体流失败:', error);
            alert('无法访问摄像头或麦克风，请检查权限设置');
            throw error;
        }
    }

    /**
     * 验证视频流质量确保一致性
     * @param {MediaStream} stream - 媒体流
     */
    validateVideoStream(stream) {
        const videoTrack = stream.getVideoTracks()[0];
        if (videoTrack) {
            const settings = videoTrack.getSettings();
            
            // 检查比例是否接近16:9，如偏离则输出警告
            if (settings.width && settings.height) {
                const aspectRatio = settings.width / settings.height;
                const expected169 = 16/9;
                const tolerance = 0.1;
                
                if (Math.abs(aspectRatio - expected169) > tolerance) {
                    console.warn(`视频比例偏离16:9标准: ${aspectRatio.toFixed(2)} vs ${expected169.toFixed(2)}`);
                }
            }
        }
    }

    /**
     * 创建新的对等连接
     * @param {string} userId - 远程用户ID
     * @param {Function} onIceCandidate - ICE候选回调
     * @returns {RTCPeerConnection}
     */
    createPeerConnection(userId, onIceCandidate) {
        const pc = new RTCPeerConnection(this.configuration);
        
        // 添加本地流
        if (this.localStream) {
            this.localStream.getTracks().forEach(track => {
                pc.addTrack(track, this.localStream);
            });
        }
        
        // 处理ICE候选
        pc.onicecandidate = (event) => {
            if (event.candidate) {
                onIceCandidate(event.candidate);
            }
        };
        
        // 处理远程流
        pc.ontrack = (event) => {
            console.log(`收到远程流 (${userId}):`, event.streams[0]);
            this.handleRemoteStream(userId, event.streams[0]);
        };
        
        // 连接状态变化
        pc.onconnectionstatechange = () => {
            console.log(`连接状态 (${userId}):`, pc.connectionState);
            this.updateConnectionStatus(userId, pc.connectionState);
            
            if (pc.connectionState === 'failed' || pc.connectionState === 'disconnected') {
                this.removePeerConnection(userId);
            }
        };
        
        this.peerConnections.set(userId, pc);
        return pc;
    }

    /**
     * 更新连接状态显示
     * @param {string} userId - 用户ID
     * @param {string} state - 连接状态
     */
    updateConnectionStatus(userId, state) {
        const wrapper = document.getElementById(`wrapper-${userId}`);
        if (wrapper) {
            let statusElement = wrapper.querySelector('.connection-status');
            if (!statusElement) {
                statusElement = document.createElement('div');
                statusElement.className = 'connection-status';
                wrapper.appendChild(statusElement);
            }
            
            statusElement.textContent = this.getStatusText(state);
            statusElement.className = `connection-status status-${state}`;
        }
    }

    /**
     * 获取状态文本
     * @param {string} state - 连接状态
     * @returns {string}
     */
    getStatusText(state) {
        const statusMap = {
            'new': '初始化中...',
            'connecting': '连接中...',
            'connected': '已连接',
            'disconnected': '连接断开',
            'failed': '连接失败',
            'closed': '连接关闭'
        };
        return statusMap[state] || state;
    }

    /**
     * 创建Offer
     * @param {string} userId - 目标用户ID
     * @returns {Promise<RTCSessionDescriptionInit>}
     */
    async createOffer(userId) {
        const pc = this.peerConnections.get(userId) || this.createPeerConnection(userId, () => {});
        
        try {
            const offer = await pc.createOffer();
            await pc.setLocalDescription(offer);
            return offer;
        } catch (error) {
            console.error('创建Offer失败:', error);
            throw error;
        }
    }

    /**
     * 创建Answer
     * @param {string} userId - 来源用户ID
     * @param {RTCSessionDescriptionInit} offer - 收到的Offer
     * @returns {Promise<RTCSessionDescriptionInit>}
     */
    async createAnswer(userId, offer) {
        const pc = this.peerConnections.get(userId) || this.createPeerConnection(userId, () => {});
        
        try {
            await pc.setRemoteDescription(offer);
            const answer = await pc.createAnswer();
            await pc.setLocalDescription(answer);
            return answer;
        } catch (error) {
            console.error('创建Answer失败:', error);
            throw error;
        }
    }

    /**
     * 处理收到的Answer
     * @param {string} userId - 来源用户ID
     * @param {RTCSessionDescriptionInit} answer - 收到的Answer
     */
    async handleAnswer(userId, answer) {
        const pc = this.peerConnections.get(userId);
        if (!pc) {
            console.error('未找到对应的PeerConnection:', userId);
            return;
        }
        
        try {
            await pc.setRemoteDescription(answer);
        } catch (error) {
            console.error('设置远程描述失败:', error);
        }
    }

    /**
     * 添加ICE候选
     * @param {string} userId - 用户ID
     * @param {RTCIceCandidateInit} candidate - ICE候选
     */
    async addIceCandidate(userId, candidate) {
        const pc = this.peerConnections.get(userId);
        if (!pc) {
            console.error('未找到对应的PeerConnection:', userId);
            return;
        }
        
        try {
            await pc.addIceCandidate(candidate);
        } catch (error) {
            console.error('添加ICE候选失败:', error);
        }
    }

    /**
     * 处理远程媒体流
     * @param {string} userId - 用户ID
     * @param {MediaStream} stream - 媒体流
     */
    handleRemoteStream(userId, stream) {
        let videoElement = document.getElementById(`video-${userId}`);
        
        if (!videoElement) {
            const remoteVideos = document.getElementById('remoteVideos');
            const videoWrapper = document.createElement('div');
            videoWrapper.className = 'remote-video-wrapper';
            videoWrapper.id = `wrapper-${userId}`;
            
            videoElement = document.createElement('video');
            videoElement.id = `video-${userId}`;
            videoElement.autoplay = true;
            videoElement.playsinline = true;
            videoElement.muted = false;
            
            // 移动端添加播放控制
            if (this.isMobile) {
                videoElement.controls = false;
                videoElement.setAttribute('webkit-playsinline', 'true');
                videoElement.setAttribute('x5-video-player-type', 'h5');
                videoElement.setAttribute('x5-video-player-fullscreen', 'false');
                
                // 添加点击播放功能
                const playButton = document.createElement('div');
                playButton.className = 'play-button';
                playButton.innerHTML = '▶️';
                playButton.onclick = () => this.playRemoteVideo(videoElement, playButton);
                videoWrapper.appendChild(playButton);
            }
            
            const label = document.createElement('div');
            label.className = 'video-label';
            const userName = document.querySelector(`#userList li[data-user-id="${userId}"]`)?.textContent || userId;
            label.textContent = userName;
            
            // 添加视频控制按钮
            const controls = document.createElement('div');
            controls.className = 'video-controls';
            
            // 放大按钮
            const expandButton = document.createElement('button');
            expandButton.className = 'expand-btn';
            expandButton.title = '放大视频';
            expandButton.innerHTML = '<span class="icon">🔍</span>';
            expandButton.onclick = () => {
                if (window.chatApp && typeof window.chatApp.expandVideo === 'function') {
                    window.chatApp.expandVideo(`video-${userId}`, userName);
                }
            };
            
            controls.appendChild(expandButton);
            
            videoWrapper.appendChild(videoElement);
            videoWrapper.appendChild(label);
            videoWrapper.appendChild(controls);
            remoteVideos.appendChild(videoWrapper);
        }
        
        videoElement.srcObject = stream;
        
        // 验证远程视频流
        this.validateVideoStream(stream);
        
        
        // 移动端强制播放
        if (this.isMobile) {
            this.attemptPlayRemoteVideo(videoElement, userId);
        }
    }

    /**
     * 尝试播放远程视频
     * @param {HTMLVideoElement} videoElement - 视频元素
     * @param {string} userId - 用户ID
     */
    async attemptPlayRemoteVideo(videoElement, userId) {
        try {
            // 延迟一下确保流已经设置
            setTimeout(async () => {
                try {
                    await videoElement.play();
                    console.log(`远程视频播放成功 (${userId})`);
                    
                    // 隐藏播放按钮
                    const wrapper = document.getElementById(`wrapper-${userId}`);
                    const playButton = wrapper?.querySelector('.play-button');
                    if (playButton) {
                        playButton.style.display = 'none';
                    }
                } catch (e) {
                    console.log(`远程视频自动播放失败 (${userId}):`, e.message);
                    // 显示播放按钮
                    const wrapper = document.getElementById(`wrapper-${userId}`);
                    const playButton = wrapper?.querySelector('.play-button');
                    if (playButton) {
                        playButton.style.display = 'flex';
                    }
                }
            }, 500);
        } catch (error) {
            console.error('播放远程视频失败:', error);
        }
    }

    /**
     * 手动播放远程视频
     * @param {HTMLVideoElement} videoElement - 视频元素
     * @param {HTMLElement} playButton - 播放按钮
     */
    async playRemoteVideo(videoElement, playButton) {
        try {
            await videoElement.play();
            playButton.style.display = 'none';
            console.log('手动播放远程视频成功');
        } catch (error) {
            console.error('手动播放远程视频失败:', error);
            alert('播放视频失败，请检查网络连接');
        }
    }

    /**
     * 移除对等连接
     * @param {string} userId - 用户ID
     */
    removePeerConnection(userId) {
        const pc = this.peerConnections.get(userId);
        if (pc) {
            pc.close();
            this.peerConnections.delete(userId);
        }
        
        // 移除视频元素
        const videoWrapper = document.getElementById(`wrapper-${userId}`);
        if (videoWrapper) {
            videoWrapper.remove();
        }
    }

    /**
     * 切换静音状态
     * @returns {boolean} - 当前静音状态
     */
    toggleMute() {
        if (!this.localStream) return false;
        
        const audioTrack = this.localStream.getAudioTracks()[0];
        if (audioTrack) {
            audioTrack.enabled = !audioTrack.enabled;
            return !audioTrack.enabled;
        }
        return false;
    }

    /**
     * 切换摄像头状态
     * @returns {boolean} - 当前摄像头状态
     */
    toggleCamera() {
        if (!this.localStream) return false;
        
        const videoTrack = this.localStream.getVideoTracks()[0];
        if (videoTrack) {
            videoTrack.enabled = !videoTrack.enabled;
            return !videoTrack.enabled;
        }
        return false;
    }

    /**
     * 清理所有连接
     */
    cleanup() {
        // 停止本地流
        if (this.localStream) {
            this.localStream.getTracks().forEach(track => track.stop());
            this.localStream = null;
        }
        
        // 关闭所有对等连接
        this.peerConnections.forEach((pc, userId) => {
            this.removePeerConnection(userId);
        });
        
        // 清空本地视频
        const localVideo = document.getElementById('localVideo');
        if (localVideo) {
            localVideo.srcObject = null;
        }
    }
} 